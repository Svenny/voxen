#pragma once

#include <compare>
#include <cstdint>

namespace voxen
{

// Helper to make `INVALID` constant work, does not define any timeline
struct InvalidTickTag {};

// Provides semantic typing for tick ID values from different timelines (defined by tag types).
// Prevents accidental comparison or arithmetic on values from incomparable timelines.
template<typename Tag>
struct TaggedTickId {
	// Any tick ID with negative value is treated as invalid, i.e. not representing any time point.
	// This constant simply allows to clearly express it in your code like `return MyTickId::INVALID`.
	constexpr static InvalidTickTag INVALID;

	constexpr TaggedTickId() = default;
	// Explicit ctor - don't accidentally cast untagged value to tagged one
	constexpr explicit TaggedTickId(int64_t val) noexcept : value(val) {}
	// Implicit helper ctor of invalid tick ID
	constexpr TaggedTickId(InvalidTickTag) noexcept : value(-1) {}

	constexpr auto operator<=>(const TaggedTickId &other) const = default;

	constexpr bool valid() const noexcept { return value >= 0; }
	constexpr bool invalid() const noexcept { return value < 0; }

	constexpr TaggedTickId operator+(int64_t d) const noexcept { return TaggedTickId(value + d); }
	constexpr TaggedTickId operator-(int64_t d) const noexcept { return TaggedTickId(value - d); }

	// Difference of two tick IDs is not a tick ID
	constexpr int64_t operator-(TaggedTickId d) const noexcept { return value - d.value; }

	constexpr TaggedTickId &operator+=(int64_t d) noexcept
	{
		value += d;
		return *this;
	}

	constexpr TaggedTickId &operator-=(int64_t d) noexcept
	{
		value -= d;
		return *this;
	}

	constexpr TaggedTickId &operator++() noexcept
	{
		++value;
		return *this;
	}

	constexpr TaggedTickId operator++(int) noexcept
	{
		TaggedTickId tick(value);
		value++;
		return tick;
	}

	int64_t value = 0;
};

} // namespace voxen
