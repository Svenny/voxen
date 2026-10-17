#pragma once

#include <tk/parse_error.hpp>

#include <cstddef>
#include <string>
#include <string_view>

// Private helpers shared by text format parsers

namespace tk::detail
{

// Returns the offset of the first byte of invalid UTF-8 sequence
// (overlong, surrogate, out of range, truncated etc.), or `npos`
size_t findInvalidUtf8(std::string_view text) noexcept;

// Decodes a code point at `offset` of valid UTF-8 text, `length` receives its size in bytes.
// Returns zero (with zero length) at the end of text.
char32_t decodeUtf8(std::string_view text, size_t offset, size_t &length) noexcept;

// Appends UTF-8 encoding of a code point, which must be a valid Unicode scalar value
void appendUtf8(std::string &out, char32_t code_point);

// Is it a valid Unicode scalar value (i.e. a code point but not a surrogate)
constexpr bool isUnicodeScalar(char32_t value) noexcept
{
	return value < 0xD800 || (value > 0xDFFF && value <= 0x10FFFF);
}

constexpr bool isAsciiDigit(char c) noexcept
{
	return c >= '0' && c <= '9';
}

constexpr bool isAsciiHexDigit(char c) noexcept
{
	return isAsciiDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

constexpr bool isAsciiAlpha(char c) noexcept
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// Value of a hex digit, `c` must be valid
constexpr uint32_t hexDigitValue(char c) noexcept
{
	if (isAsciiDigit(c)) {
		return uint32_t(c - '0');
	}

	return uint32_t((c | 0x20) - 'a' + 10);
}

// Parses a decimal floating-point number with `std::from_chars`, flushing underflow
// to (signed) zero instead of failing. `text` must be a well-formed number like
// `[-]digits[.digits][e[+-]digits]`, either digits part can be empty but not both.
// Returns false on overflow.
bool parseDecimalFloat(std::string_view text, double &value) noexcept;

// Makes an error with line/column computed from byte offset in `text`
ParseError makeParseError(std::string_view text, size_t offset, std::string message);

} // namespace tk::detail
