#pragma once

#include <algorithm>
#include <bit>
#include <cassert>
#include <limits>
#include <optional>
#include <vector>
#include <utility>

namespace extras
{

// A simple implementation of free range list based linear allocator.
// Intended to be inherited from (using CRTP idiom) or used in composition.
// `S` is unsigned type used in address arithmetic (defines allocation space).
// `G` is allocation granularity - a power of two, each size and alignment is rounded up to it.
// Base class (`B` template argument) must have public method
// `static void on_allocator_freed(linear_allocator<...> &) noexcept`
// This is informational callback called when the last object was freed.
// NOTE: this callback is never called from object's destructor.
template<typename B, typename S, S G = 64>
class linear_allocator {
public:
	static_assert(std::is_unsigned_v<S>, "Size type must be unsigned integer");
	static_assert(std::has_single_bit(G), "Allocation granularity must be a power of two");

	using size_type = S;
	using range_type = std::pair<size_type, size_type>;

	linear_allocator(size_type full_size, size_type reserve_blocks = 128) : m_full_size(full_size)
	{
		// Limit the size for extra safety margin in offset arithmetic
		assert(full_size <= std::numeric_limits<size_type>::max() / 2);

		m_free_ranges.reserve(reserve_blocks);
		m_free_ranges.emplace_back(0, full_size);
	}

	linear_allocator(linear_allocator &&) = delete;
	linear_allocator(const linear_allocator &) = delete;
	linear_allocator &operator = (linear_allocator &&) = delete;
	linear_allocator &operator = (const linear_allocator &) = delete;

	~linear_allocator() = default;

	[[nodiscard]] std::optional<range_type> allocate(size_type size, size_type align)
	{
		constexpr size_type granularity = G;

		assert(size > 0 && align > 0);
		size = align_up(size, granularity);
		align = std::max(align, granularity);

		if (size > m_full_size) [[unlikely]] {
			// Outright reject unfeasibly big requests
			return std::nullopt;
		}

		for (size_t i = 0; i < m_free_ranges.size(); i++) {
			const size_type begin = m_free_ranges[i].first;
			const size_type end = m_free_ranges[i].second;

			const size_type adjusted_begin = align_up(begin, align);
			const size_type adjusted_end = adjusted_begin + size;

			if (adjusted_end > end) {
				// This block is too small for requested size/alignment
				continue;
			}

			// Ensure there is room for at least two more objects.
			// First one can be emplaced down the code. If allocation fails here,
			// we won't modify the vector, thus ensuring strong exception safety.
			// Second one can be emplaced during `free(adjusted_begin, adjusted_end)`,
			// so this reservation guarantees that `free()` is actually noexcept.
			// TODO (Svenny): but it seems to force linear (not exponential) capacity growth.
			// ...but we don't need to care about it unless free blocks number is increasing.
			m_free_ranges.reserve(m_free_ranges.size() + 2);

			bool used_inplace = false;

			if (adjusted_end < end) {
				m_free_ranges[i] = range_type(adjusted_end, end);
				used_inplace = true;
			}

			if (adjusted_begin > begin) {
				if (!used_inplace) {
					m_free_ranges[i] = range_type(begin, adjusted_begin);
					used_inplace = true;
				} else {
					m_free_ranges.emplace(m_free_ranges.begin() + ptrdiff_t(i), begin, adjusted_begin);
				}
			}

			if (!used_inplace) {
				m_free_ranges.erase(m_free_ranges.begin() + ptrdiff_t(i));
			}

			return range_type(adjusted_begin, adjusted_end);
		}

		// No free range satisfying the request
		return std::nullopt;
	}

	// Take all the range by a single allocation
	[[nodiscard]] range_type allocate_all() noexcept
	{
		assert(is_free());
		m_free_ranges.clear();
		return range_type(0, m_full_size);
	}

	[[nodiscard]] std::optional<range_type> grow(range_type range, size_type addendum)
	{
		if (addendum == 0) [[unlikely]] {
			return range;
		}

		const size_type begin = range.first;
		const size_type end = range.second;
		assert(begin < end);

		// Find the first "higher" free block and try to "eat" it
		auto iter = std::upper_bound(m_free_ranges.begin(), m_free_ranges.end(), std::make_pair(begin, end));

		if (iter == m_free_ranges.end()) {
			// No "higher" block
			return std::nullopt;
		}

		if (iter->first != end) {
			// "Higher" block is not adjacent to `range`
			return std::nullopt;
		}

		const size_type desired_end = end + align_up(addendum, G);
		if (iter->second < desired_end) {
			// "Higher" block is not enough to satisfy the request
			return std::nullopt;
		}

		if (iter->second == desired_end) [[unlikely]] {
			// We've "ate" the whole block
			m_free_ranges.erase(iter);
		} else {
			// Something left of this block
			iter->first = desired_end;
		}

		return range_type(begin, desired_end);
	}

	void free(range_type range) noexcept
	{
		const size_type begin = range.first;
		const size_type end = range.second;
		assert(begin < end);

		// Find the first "higher" free block and insert a new one right before it
		auto iter = std::upper_bound(m_free_ranges.begin(), m_free_ranges.end(), std::make_pair(begin, end));
		// This can't throw as `allocate` has reserved space for this block in advance
		iter = m_free_ranges.emplace(iter, begin, end);

		if (auto next = iter + 1; next != m_free_ranges.end()) {
			assert(next->first >= end);
			if (next->first == end) {
				// The next free range starts right at the end of just added one - merge them
				iter->second = next->second;
				// This can't invalidate `iter`, as well as all iterators before it
				m_free_ranges.erase(next);
			}
		}

		if (iter != m_free_ranges.begin()) {
			auto prev = iter - 1;
			assert(prev->second <= begin);
			if (prev->second == begin) {
				// The previous free range ends right at the start of just added one - merge them
				iter->first = prev->first;
				// This will invalidate `iter`, but we are not going to use it anymore
				m_free_ranges.erase(prev);
			}
		}

		if (is_free()) [[unlikely]] {
			// Don't do anything after this call, `this` is probably destroyed in it
			B::on_allocator_freed(*this);
		}
	}

	// Automatically free all made allocations
	void reset() noexcept
	{
		if (!is_free()) {
			m_free_ranges.clear();
			m_free_ranges.emplace_back(0, m_full_size);
			B::on_allocator_freed(*this);
		}
	}

	[[nodiscard]] bool is_free() const noexcept
	{
		return m_free_ranges.size() == 1 && m_free_ranges[0] == range_type(0, m_full_size);
	}

protected:
	// Holds [begin; end) pairs of offsets into the allocation.
	// This vector is always ordered by both fields of its elements, that is,
	// i+1'th element has both `.first` and `.second` greater than in i'th.
	std::vector<range_type> m_free_ranges;
	// Full size of the memory block this allocator is suballocating.
	const size_type m_full_size = 0;

private:
	static size_type align_up(size_type size, size_type alignment) noexcept
	{
		return (size + alignment - 1) & ~(alignment - 1);
	}
};

}
