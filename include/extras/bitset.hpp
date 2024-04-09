#pragma once

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace extras
{

// An alternative for `std::bitset` with advanced features needed for
// object pool implementation (or other techniques employing "free list")
template<size_t N>
class bitset final {
public:
	static_assert(N > 0);

	// Default contructor sets all bits to zero
	bitset() noexcept { clear(); }
	// Set all bits to `value`
	explicit bitset(bool value) noexcept
	{
		if (value) {
			set();
		} else {
			clear();
		}
	}

	bitset(bitset &&other) = default;
	bitset(const bitset &other) = default;
	bitset &operator=(bitset &&other) = default;
	bitset &operator=(const bitset &other) = default;
	~bitset() = default;

	// Return bit value at the given index
	bool test(size_t pos) const noexcept
	{
		assert(pos < N);
		return (m_data[pos / 64] & (uint64_t(1) << (pos % 64))) != 0;
	}

	// Set bit at the given index to one
	void set(size_t pos) noexcept
	{
		assert(pos < N);
		m_data[pos / 64] |= (uint64_t(1) << (pos % 64));
	}

	// Set all bits to ones
	void set() noexcept
	{
		if constexpr (ALL_BITS_USED) {
			// No padding bits, just fill everything with ones
			std::fill_n(m_data, NUM_INTS, UINT64_MAX);
		} else {
			// Padding bits must not be filled with ones to avoid breaking `popcount()`
			std::fill_n(m_data, NUM_INTS - 1, UINT64_MAX);
			// First K bits will be ones, the rest is zero
			m_data[NUM_INTS - 1] = (uint64_t(1) << LAST_USED_BITS) - 1;
		}
	}

	// Set bit at the given index to zero
	void clear(size_t pos) noexcept
	{
		assert(pos < N);
		m_data[pos / 64] &= ~(uint64_t(1) << (pos % 64));
	}

	// Set all bits to zero
	void clear() noexcept { memset(m_data, 0, sizeof(m_data)); }

	// Return the number of bits set to one
	size_t popcount() const noexcept
	{
		size_t result = 0;
		for (uint64_t item : m_data) {
			result += size_t(std::popcount(item));
		}
		return result;
	}

	// Return index of the first zero bit or `SIZE_MAX` if all bits are ones
	size_t first_zero() const noexcept
	{
		constexpr size_t limit = ALL_BITS_USED ? NUM_INTS : NUM_INTS - 1;
		for (size_t i = 0; i < limit; i++) {
			auto cnt = size_t(std::countr_one(m_data[i]));
			if (cnt < 64) {
				return i * 64 + cnt;
			}
		}

		if constexpr (!ALL_BITS_USED) {
			auto cnt = size_t(std::countr_one(m_data[NUM_INTS - 1]));
			if (cnt < LAST_USED_BITS) {
				return (NUM_INTS - 1) * 64 + cnt;
			}
		}

		return SIZE_MAX;
	}

	// Turn the first zero bit into one and return its index.
	// Does nothing and returns `SIZE_MAX` if all bits are ones.
	size_t occupy_zero() noexcept
	{
		constexpr size_t limit = ALL_BITS_USED ? NUM_INTS : NUM_INTS - 1;
		for (size_t i = 0; i < limit; i++) {
			auto cnt = size_t(std::countr_one(m_data[i]));
			if (cnt < 64) {
				m_data[i] |= (uint64_t(1) << cnt);
				return i * 64 + cnt;
			}
		}

		if constexpr (!ALL_BITS_USED) {
			auto cnt = size_t(std::countr_one(m_data[NUM_INTS - 1]));
			if (cnt < LAST_USED_BITS) {
				m_data[NUM_INTS - 1] |= (uint64_t(1) << cnt);
				return (NUM_INTS - 1) * 64 + cnt;
			}
		}

		return SIZE_MAX;
	}

private:
	constexpr static size_t NUM_INTS = (N + 63) / 64;
	constexpr static bool ALL_BITS_USED = (N % 64 == 0);
	constexpr static size_t LAST_USED_BITS = N % 64;
	uint64_t m_data[NUM_INTS];
};

} // namespace extras
