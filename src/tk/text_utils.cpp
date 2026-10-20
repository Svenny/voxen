module;

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

module tk.text_utils;

namespace tk::detail
{

size_t findInvalidUtf8(std::string_view text) noexcept
{
	const auto *bytes = reinterpret_cast<const unsigned char *>(text.data());
	const size_t size = text.size();

	size_t i = 0;
	while (i < size) {
		const unsigned char lead = bytes[i];

		if (lead < 0x80) {
			i++;
			continue;
		}

		size_t length;
		// Allowed range for the second byte, it catches overlong
		// encodings, surrogates and values above U+10FFFF
		unsigned char second_min = 0x80;
		unsigned char second_max = 0xBF;

		if (lead >= 0xC2 && lead <= 0xDF) {
			length = 2;
		} else if (lead >= 0xE0 && lead <= 0xEF) {
			length = 3;
			if (lead == 0xE0) {
				second_min = 0xA0;
			} else if (lead == 0xED) {
				second_max = 0x9F;
			}
		} else if (lead >= 0xF0 && lead <= 0xF4) {
			length = 4;
			if (lead == 0xF0) {
				second_min = 0x90;
			} else if (lead == 0xF4) {
				second_max = 0x8F;
			}
		} else {
			return i;
		}

		if (size - i < length) {
			return i;
		}

		if (bytes[i + 1] < second_min || bytes[i + 1] > second_max) {
			return i;
		}

		for (size_t j = 2; j < length; j++) {
			if ((bytes[i + j] & 0xC0) != 0x80) {
				return i;
			}
		}

		i += length;
	}

	return std::string_view::npos;
}

char32_t decodeUtf8(std::string_view text, size_t offset, size_t &length) noexcept
{
	if (offset >= text.size()) {
		length = 0;
		return 0;
	}

	const auto lead = static_cast<unsigned char>(text[offset]);

	if (lead < 0x80) {
		length = 1;
		return lead;
	}

	uint32_t cp;
	if (lead < 0xE0) {
		length = 2;
		cp = lead & 0x1F;
	} else if (lead < 0xF0) {
		length = 3;
		cp = lead & 0x0F;
	} else {
		length = 4;
		cp = lead & 0x07;
	}

	length = std::min(length, text.size() - offset);
	for (size_t i = 1; i < length; i++) {
		cp = (cp << 6) | (static_cast<unsigned char>(text[offset + i]) & 0x3F);
	}

	return char32_t(cp);
}

void appendUtf8(std::string &out, char32_t code_point)
{
	const auto cp = uint32_t(code_point);

	if (cp < 0x80) {
		out += char(cp);
	} else if (cp < 0x800) {
		out += char(0xC0 | (cp >> 6));
		out += char(0x80 | (cp & 0x3F));
	} else if (cp < 0x10000) {
		out += char(0xE0 | (cp >> 12));
		out += char(0x80 | ((cp >> 6) & 0x3F));
		out += char(0x80 | (cp & 0x3F));
	} else {
		out += char(0xF0 | (cp >> 18));
		out += char(0x80 | ((cp >> 12) & 0x3F));
		out += char(0x80 | ((cp >> 6) & 0x3F));
		out += char(0x80 | (cp & 0x3F));
	}
}

bool parseDecimalFloat(std::string_view text, double &value) noexcept
{
	auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
	if (ec != std::errc::result_out_of_range) {
		return ec == std::errc();
	}

	// Both overflow and underflow are reported as out of range. Tell them
	// apart by the decimal exponent of the leading nonzero digit, it is
	// far from zero for any out of range value so precision doesn't matter.
	const bool negative = text.starts_with('-');
	const size_t mantissa_start = negative ? 1 : 0;
	const size_t exponent_start = std::min(text.find_first_of("eE"), text.size());
	const std::string_view mantissa = text.substr(mantissa_start, exponent_start - mantissa_start);

	const size_t point = std::min(mantissa.find('.'), mantissa.size());
	// Out of range value can't be zero, there must be a nonzero digit
	const size_t leading = mantissa.find_first_not_of("0.");
	int64_t exponent = leading < point ? int64_t(point - leading) - 1 : int64_t(point) - int64_t(leading);

	if (exponent_start < text.size()) {
		std::string_view exponent_text = text.substr(exponent_start + 1);
		const bool exponent_negative = exponent_text.starts_with('-');
		if (exponent_text.starts_with('-') || exponent_text.starts_with('+')) {
			exponent_text.remove_prefix(1);
		}

		int64_t explicit_exponent = 0;
		for (char c : exponent_text) {
			// Saturate, the exact value doesn't matter this far out of range
			explicit_exponent = std::min<int64_t>(explicit_exponent * 10 + (c - '0'), 1'000'000'000);
		}

		exponent += exponent_negative ? -explicit_exponent : explicit_exponent;
	}

	if (exponent >= 0) {
		return false;
	}

	value = negative ? -0.0 : 0.0;
	return true;
}

ParseError makeParseError(std::string_view text, size_t offset, std::string message)
{
	offset = std::min(offset, text.size());

	uint32_t line = 1;
	uint32_t column = 1;

	for (size_t i = 0; i < offset; i++) {
		const auto c = static_cast<unsigned char>(text[i]);

		if (c == '\n') {
			line++;
			column = 1;
		} else if ((c & 0xC0) != 0x80) {
			// Count only lead bytes of UTF-8 sequences
			column++;
		}
	}

	return ParseError { .message = std::move(message), .line = line, .column = column };
}

} // namespace tk::detail
