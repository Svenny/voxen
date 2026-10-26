#pragma once

#include <tk/hash.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace tk
{

// Universal identifier, a random 128-bit value.
// Can be attached to any entity in the engine or game to uniquely locate it.
// The string format is a contract shared by the engine, tools and data files.
// fmt formatter is available from `tk/format.hpp`.
struct UID {
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
	constexpr void toChars(std::span<char, CHAR_REPR_LENGTH> out) const noexcept;

	// Parse string representation written by `toChars`, without the null terminator.
	// Accepts only exactly that format, e.g. no uppercase hex digits.
	constexpr static std::optional<UID> tryParse(std::string_view in) noexcept;
	// Same as `tryParse` but returns a human-readable error message on failure
	static std::expected<UID, std::string> fromChars(std::string_view in);

	// Generate non-deterministic random UID (using `tk::rng::entropySeed()`)
	static UID generateRandom();

	uint64_t v0 = 0;
	uint64_t v1 = 0;
};

constexpr void UID::toChars(std::span<char, CHAR_REPR_LENGTH> out) const noexcept
{
	constexpr char DIGITS[] = "0123456789abcdef";

	auto encode_u32 = [&](size_t pos, uint32_t value) {
		for (size_t i = 0; i < 8; i++) {
			out[pos + i] = DIGITS[(value >> (28u - 4u * i)) & 0xFu];
		}
	};

	encode_u32(0, static_cast<uint32_t>(v0 >> 32u));
	out[8] = '-';
	encode_u32(9, static_cast<uint32_t>(v0));
	out[17] = '-';
	encode_u32(18, static_cast<uint32_t>(v1 >> 32u));
	out[26] = '-';
	encode_u32(27, static_cast<uint32_t>(v1));
	out[35] = '\0';
}

constexpr std::optional<UID> UID::tryParse(std::string_view in) noexcept
{
	if (in.size() != CHAR_REPR_LENGTH - 1 || in[8] != '-' || in[17] != '-' || in[26] != '-') {
		return std::nullopt;
	}

	bool ok = true;

	auto decode_u64 = [&](std::string_view x) {
		auto decode_char = [&](char c) {
			if (c >= '0' && c <= '9') {
				return c - '0';
			} else if (c >= 'a' && c <= 'f') {
				return c - 'a' + 10;
			}
			ok = false;
			return 0;
		};

		uint64_t result = 0;
		for (char c : x.substr(0, 8)) {
			result = (result << 4u) | uint64_t(decode_char(c));
		}

		for (char c : x.substr(9, 8)) {
			result = (result << 4u) | uint64_t(decode_char(c));
		}

		return result;
	};

	UID uid(decode_u64(in.substr(0, 17)), decode_u64(in.substr(18, 17)));
	if (!ok) {
		return std::nullopt;
	}

	return uid;
}

consteval UID::UID(std::span<const char, CHAR_REPR_LENGTH> in) noexcept
{
	// Throwing in a consteval context gives a compile error
	auto fail = []() { throw "wrong input format"; };

	std::optional<UID> uid = tryParse(std::string_view(in.data(), CHAR_REPR_LENGTH - 1));
	if (in[CHAR_REPR_LENGTH - 1] != '\0' || !uid) {
		fail();
		return;
	}

	v0 = uid->v0;
	v1 = uid->v1;
}

// "Extend" a small-dimension (64 bits or smaller) key with whatever meaning to UID.
// `pivot` should be a (fixed) random UID, it defines a subsystem (domain) where
// this key belongs to. The idea is that if the same key is used in different domains
// it will be converted to different UIDs using this pivot, eliminating UID collisions.
constexpr UID deriveUid(UID pivot, uint64_t key) noexcept
{
	pivot.v1 ^= hash::xxh64Fixed(pivot.v0 ^ key);
	return pivot;
}

} // namespace tk

namespace std
{

template<>
struct hash<tk::UID> {
	size_t operator()(const tk::UID &uid) const noexcept
	{
		// No special hashing is needed, UIDs are already random
		return static_cast<size_t>(uid.v0 ^ uid.v1);
	}
};

} // namespace std
