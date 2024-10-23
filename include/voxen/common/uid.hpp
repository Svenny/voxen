#pragma once

#include <voxen/visibility.hpp>

#include <fmt/base.h>

#include <cstdint>
#include <functional>
#include <span>

namespace voxen
{

// Universal identifier, a random 128-bit value.
// Can be attached to any entity in the engine or game to uniquely locate it.
// This class can be used without any engine initialization.
struct VOXEN_API UID {
	// String representation consists of 4 values of 8 hex digits (32 bits),
	// three dashes between them and a null terminator.
	// Null terminator is included in the count to simplify code in common
	// usage cases like `UID("...")` or `char buf[...]; uid.toChars(buf);`.
	constexpr static size_t CHAR_REPR_LENGTH = 4 * 8 + 3 + 1;

	UID() = default;
	constexpr UID(uint64_t v0, uint64_t v1) noexcept : v0(v0), v1(v1) {}
	// Constructor has full compile-time input validation and will only
	// accept strings formatted exactly like the result of `toChars`.
	// Other inputs will cause compile-time errors.
	consteval UID(std::span<const char, CHAR_REPR_LENGTH> in) noexcept;
	UID(UID &&) = default;
	UID(const UID &) = default;
	UID &operator=(UID &&) = default;
	UID &operator=(const UID &) = default;
	~UID() = default;

	auto operator<=>(const UID &) const = default;

	// Write string representation with the following format:
	// "########-########-########-########\0"
	// "v0 upper-v0 lower-v1 upper-v1 lower"
	// where # are lowercase hex characters ('0'-'9' and 'a'-'f')
	void toChars(std::span<char, CHAR_REPR_LENGTH> out) const noexcept;

	// Generate non-deterministic random UID (using `std::random_device`)
	static UID generateRandom();

	uint64_t v0 = 0;
	uint64_t v1 = 0;
};

consteval UID::UID(std::span<const char, CHAR_REPR_LENGTH> in) noexcept
{
	auto fail = []() { throw "wrong input format"; };

	if (in[8] != '-' || in[17] != '-' || in[26] != '-') {
		fail();
	}

	auto decode_u64 = [&](std::span<const char, 17> x) {
		auto decode_char = [&](char c) {
			if (c >= '0' && c <= '9') {
				return c - '0';
			} else if (c >= 'a' && c <= 'f') {
				return c - 'a' + 10;
			}
			fail();
			return -1;
		};

		uint64_t result = 0;
		for (char c : x.subspan<0, 8>()) {
			result = (result << 4u) | uint64_t(decode_char(c));
		}

		for (char c : x.subspan<9, 8>()) {
			result = (result << 4u) | uint64_t(decode_char(c));
		}

		return result;
	};

	v0 = decode_u64(in.subspan<0, 17>());
	v1 = decode_u64(in.subspan<18, 17>());
}

} // namespace voxen

namespace fmt
{

template<>
struct VOXEN_API formatter<voxen::UID> : formatter<string_view> {
	format_context::iterator format(voxen::UID id, format_context &ctx) const;
};

} // namespace fmt

namespace std
{

template<>
struct hash<voxen::UID> {
	size_t operator()(const voxen::UID &uid) const noexcept
	{
		// No special hashing is needed, UIDs are already random
		return static_cast<size_t>(uid.v0 ^ uid.v1);
	}
};

} // namespace std
