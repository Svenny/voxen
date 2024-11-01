#include <voxen/debug/uid_registry.hpp>

#include <voxen/os/futex.hpp>

#include <fmt/format.h>

#include <cstring>
#include <limits>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <utility>

namespace voxen::debug
{

namespace
{

// Hybrid owning/non-owning string view, eliminates copies of string literals.
// Quite an unsafe thing, we should not expose it outside.
class StringOrLiteral {
public:
	constexpr static int LENGTH_BITS = std::numeric_limits<size_t>::digits - 1;

	StringOrLiteral(std::string_view view, bool need_copy) : m_is_copied(need_copy), m_length(view.length())
	{
		if (need_copy) {
			size_t len = view.length();

			char *ptr = new char[len + 1];
			std::memcpy(ptr, view.data(), len);
			ptr[len] = '\0';

			m_data = ptr;
		} else {
			m_data = view.data();
		}
	}

	StringOrLiteral(StringOrLiteral &&other) noexcept
		: m_data(other.m_data), m_is_copied(other.m_is_copied), m_length(other.m_length)
	{
		other.m_data = nullptr;
		other.m_is_copied = 0;
		other.m_length = 0;
	}

	StringOrLiteral &operator=(StringOrLiteral &&other) noexcept
	{
		if (this != &other) {
			if (m_is_copied) {
				delete[] m_data;
			}

			m_data = std::exchange(other.m_data, nullptr);
			m_is_copied = other.m_is_copied;
			m_length = other.m_length;

			other.m_is_copied = 0;
			other.m_length = 0;
		}

		return *this;
	}

	StringOrLiteral(const StringOrLiteral &) = delete;
	StringOrLiteral &operator=(const StringOrLiteral &) = delete;

	~StringOrLiteral() noexcept
	{
		if (m_is_copied) {
			delete[] m_data;
		}
	}

	operator std::string_view() const noexcept { return { m_data, m_length }; }

private:
	const char *m_data = nullptr;
	size_t m_is_copied : 1 = 0;
	size_t m_length : LENGTH_BITS = 0;
};

struct DataShard {
	os::FutexRWLock lock;
	// TODO: use more memory-efficient container, maybe something like HAMT (+ less sparse string storage?)
	// This feature is debug-only and we want its memory footprint to be as small as possible.
	std::unordered_map<UID, StringOrLiteral> data;
};

// More shards reduce lock contention but waste more memory
constexpr uint64_t NUM_SHARDS = 32;

DataShard g_shards[NUM_SHARDS];

DataShard &selectShard(UID id) noexcept
{
	return g_shards[id.v1 % NUM_SHARDS];
}

} // namespace

void UidRegistry::registerLiteral(UID id, std::string_view view)
{
	auto &shard = selectShard(id);
	std::lock_guard lk(shard.lock);
	shard.data.insert_or_assign(id, StringOrLiteral(view, false));
}

void UidRegistry::registerString(UID id, std::string_view view)
{
	auto &shard = selectShard(id);
	std::lock_guard lk(shard.lock);
	shard.data.insert_or_assign(id, StringOrLiteral(view, true));
}

void UidRegistry::unregister(UID id) noexcept
{
	auto &shard = selectShard(id);
	std::lock_guard lk(shard.lock);
	shard.data.erase(id);
}

void UidRegistry::lookup(UID id, std::string &out, Format format)
{
	out.clear();

	auto &shard = selectShard(id);
	std::shared_lock lk(shard.lock);

	auto iter = shard.data.find(id);

	if (iter != shard.data.end()) {
		if (format == FORMAT_STRING_AND_UID) {
			out = fmt::format("{} ({})", std::string_view(iter->second), id);
		} else {
			out = std::string_view(iter->second);
		}
	} else {
		if (format != FORMAT_STRING_ONLY) {
			char uid_chars[UID::CHAR_REPR_LENGTH];
			id.toChars(uid_chars);
			out = uid_chars;
		}
	}
}

} // namespace voxen::debug
