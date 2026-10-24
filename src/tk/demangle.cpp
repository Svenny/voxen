#include <tk/demangle.hpp>

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <new>
#include <utility>
#include <vector>

// The demangler config includes libc++abi's `abort_message.h` to define this
// macro for libc++ headers unless it's already defined. We don't use libc++.
#define _LIBCPP_VERBOSE_ABORT(...) std::abort()
#include <demangle/ItaniumDemangle.h>

// The only out-of-line part of the demangler core, upstream defines it in libc++abi's `cxa_demangle.cpp`
// next to `__cxa_demangle`. Skips a discriminator (`_<digit>`, `__<digits>_` or trailing `<digits>`).
const char *itanium_demangle::parse_discriminator(const char *first, const char *last)
{
	if (first == last) {
		return first;
	}

	auto is_digit = [](char c) { return c >= '0' && c <= '9'; };

	if (*first == '_') {
		const char *t = first + 1;
		if (t != last) {
			if (is_digit(*t)) {
				return t + 1;
			}

			if (*t == '_') {
				for (++t; t != last && is_digit(*t); ++t) {
				}
				if (t != last && *t == '_') {
					return t + 1;
				}
			}
		}
	} else if (is_digit(*first)) {
		const char *t = first + 1;
		for (; t != last && is_digit(*t); ++t) {
		}
		if (t == last) {
			return last;
		}
	}

	return first;
}

namespace tk
{

namespace
{

using itanium_demangle::Node;

// Arena for AST nodes, they are never destroyed individually
class NodeAllocator {
public:
	template<typename T, typename... Args>
	T *makeNode(Args &&...args)
	{
		return new (allocate(sizeof(T))) T(std::forward<Args>(args)...);
	}

	void *allocateNodeArray(size_t size) { return allocate(size * sizeof(Node *)); }

private:
	constexpr static size_t BLOCK_SIZE = 4096;
	// Sufficient for every node type, also the alignment of `new std::byte[]`
	constexpr static size_t ALIGNMENT = alignof(std::max_align_t);

	std::vector<std::unique_ptr<std::byte[]>> m_blocks;
	std::byte *m_block_end = nullptr;
	size_t m_block_free = 0;

	void *allocate(size_t size)
	{
		size = (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);

		if (size > BLOCK_SIZE / 4) {
			// Large allocation, give it a separate block and keep using the current one
			return m_blocks.emplace_back(std::make_unique_for_overwrite<std::byte[]>(size)).get();
		}

		if (size > m_block_free) {
			m_block_end = m_blocks.emplace_back(std::make_unique_for_overwrite<std::byte[]>(BLOCK_SIZE)).get()
				+ BLOCK_SIZE;
			m_block_free = BLOCK_SIZE;
		}

		m_block_free -= size;
		return m_block_end - m_block_free - size;
	}
};

using Parser = itanium_demangle::ManglingParser<NodeAllocator>;

} // namespace

std::optional<std::string> demangle(std::string_view name)
{
	// The parser also accepts bare type manglings, e.g. "i" would become "int"
	if (!name.starts_with("_Z")) {
		return std::nullopt;
	}

	Parser parser(name.data(), name.data() + name.size());
	Node *ast = parser.parse();
	if (!ast) {
		return std::nullopt;
	}

	itanium_demangle::OutputBuffer output;
	ast->print(output);

	std::string result(output.getBuffer(), output.getCurrentPosition());
	std::free(output.getBuffer());
	return result;
}

} // namespace tk
