#include <tk/json.hpp>

#include "text_utils_priv.hpp"

#include <charconv>
#include <limits>
#include <optional>
#include <system_error>
#include <unordered_set>

namespace tk::json
{

Array::Array(std::initializer_list<Value> values) : m_values(values) {}

Value &Array::push_back(Value value)
{
	return m_values.emplace_back(std::move(value));
}

bool Array::operator==(const Array &other) const noexcept
{
	return m_values == other.m_values;
}

Object::Object(std::initializer_list<Item> items)
{
	for (const Item &item : items) {
		insertOrAssign(item.first, item.second);
	}
}

Value *Object::find(std::string_view key) noexcept
{
	for (Item &item : m_items) {
		if (item.first == key) {
			return &item.second;
		}
	}

	return nullptr;
}

const Value *Object::find(std::string_view key) const noexcept
{
	return const_cast<Object *>(this)->find(key);
}

Value &Object::insertOrAssign(std::string key, Value value)
{
	if (Value *existing = find(key); existing) {
		*existing = std::move(value);
		return *existing;
	}

	return m_items.emplace_back(std::move(key), std::move(value)).second;
}

bool Object::operator==(const Object &other) const noexcept
{
	if (m_items.size() != other.m_items.size()) {
		return false;
	}

	for (const Item &item : m_items) {
		const Value *other_value = other.find(item.first);
		if (!other_value || !(*other_value == item.second)) {
			return false;
		}
	}

	return true;
}

std::optional<double> Value::number() const noexcept
{
	if (const int64_t *integer = get<int64_t>(); integer) {
		return double(*integer);
	}

	if (const double *number = get<double>(); number) {
		return *number;
	}

	return std::nullopt;
}

namespace detail
{

using tk::detail::appendUtf8;
using tk::detail::decodeUtf8;
using tk::detail::hexDigitValue;
using tk::detail::isAsciiAlpha;
using tk::detail::isAsciiDigit;
using tk::detail::isAsciiHexDigit;
using tk::detail::isUnicodeScalar;

class Parser {
public:
	Parser(std::string_view text, Dialect dialect) noexcept : m_text(text), m_json5(dialect == Dialect::Json5) {}

	std::expected<Value, ParseError> run()
	{
		if (size_t bad = tk::detail::findInvalidUtf8(m_text); bad != std::string_view::npos) {
			return std::unexpected(tk::detail::makeParseError(m_text, bad, "invalid UTF-8"));
		}

		try {
			// Skip UTF-8 BOM if present (JSON5 treats it as whitespace anyway)
			if (m_text.starts_with("\xEF\xBB\xBF")) {
				m_pos = 3;
			}

			skipWhitespace();
			Value value = parseValue();
			skipWhitespace();

			if (!atEnd()) {
				fail("unexpected content after the value");
			}

			return value;
		}
		catch (Failure &failure) {
			return std::unexpected(tk::detail::makeParseError(m_text, failure.offset, std::move(failure.message)));
		}
	}

private:
	// Recursion limit for nested arrays and objects
	constexpr static int32_t MAX_NESTING_DEPTH = 128;
	// Objects of this size and larger use a hash set to find duplicate keys,
	// linear search is faster for smaller ones
	constexpr static size_t HASHED_KEYS_THRESHOLD = 32;

	struct Failure {
		size_t offset;
		std::string message;
	};

	std::string_view m_text;
	size_t m_pos = 0;
	int32_t m_depth = 0;
	bool m_json5;

	[[noreturn]] void fail(std::string message, size_t offset) { throw Failure { offset, std::move(message) }; }
	[[noreturn]] void fail(std::string message) { fail(std::move(message), m_pos); }

	bool atEnd() const noexcept { return m_pos >= m_text.size(); }

	// Returns '\0' past the end of text
	char peek(size_t ahead = 0) const noexcept { return m_pos + ahead < m_text.size() ? m_text[m_pos + ahead] : '\0'; }

	bool lookingAt(std::string_view s) const noexcept { return m_text.substr(m_pos).starts_with(s); }

	static bool isJson5LineTerminator(char32_t cp) noexcept
	{
		return cp == '\n' || cp == '\r' || cp == 0x2028 || cp == 0x2029;
	}

	// JSON5 allows all of ECMAScript whitespace and line terminators
	static bool isJson5Space(char32_t cp) noexcept
	{
		switch (cp) {
		case '\t':
		case '\n':
		case '\v':
		case '\f':
		case '\r':
		case ' ':
		case 0x00A0:
		case 0x1680:
		case 0x2028:
		case 0x2029:
		case 0x202F:
		case 0x205F:
		case 0x3000:
		case 0xFEFF:
			return true;
		default:
			return cp >= 0x2000 && cp <= 0x200A;
		}
	}

	// Code point at the current position, `length` receives its size in bytes
	char32_t peekCodePoint(size_t &length) const noexcept { return decodeUtf8(m_text, m_pos, length); }

	void skipWhitespace()
	{
		while (!atEnd()) {
			const char c = peek();

			if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
				m_pos++;
				continue;
			}

			if (!m_json5) {
				return;
			}

			if (lookingAt("//")) {
				while (!atEnd()) {
					size_t length = 0;
					if (isJson5LineTerminator(peekCodePoint(length))) {
						break;
					}
					m_pos += length;
				}
				continue;
			}

			if (lookingAt("/*")) {
				const size_t end = m_text.find("*/", m_pos + 2);
				if (end == std::string_view::npos) {
					fail("unterminated comment");
				}
				m_pos = end + 2;
				continue;
			}

			size_t length = 0;
			if (!isJson5Space(peekCodePoint(length))) {
				return;
			}
			m_pos += length;
		}
	}

	void enterNested()
	{
		if (++m_depth > MAX_NESTING_DEPTH) {
			fail("nesting is too deep");
		}
	}

	Value parseValue()
	{
		if (atEnd()) {
			fail("expected a value");
		}

		const char c = peek();

		if (c == '{') {
			return parseObject();
		}

		if (c == '[') {
			return parseArray();
		}

		if (c == '"' || (m_json5 && c == '\'')) {
			return parseString();
		}

		if (lookingAt("true")) {
			m_pos += 4;
			return true;
		}

		if (lookingAt("false")) {
			m_pos += 5;
			return false;
		}

		if (lookingAt("null")) {
			m_pos += 4;
			return nullptr;
		}

		if (c == '-' || isAsciiDigit(c) || (m_json5 && (c == '+' || c == '.' || c == 'I' || c == 'N'))) {
			return parseNumber();
		}

		fail("expected a value");
	}

	Value parseNumber()
	{
		const size_t start = m_pos;
		bool negative = false;

		if (peek() == '-' || (m_json5 && peek() == '+')) {
			negative = peek() == '-';
			m_pos++;
		}

		if (m_json5) {
			if (lookingAt("Infinity")) {
				m_pos += 8;
				constexpr double INF = std::numeric_limits<double>::infinity();
				return negative ? -INF : INF;
			}

			if (lookingAt("NaN")) {
				m_pos += 3;
				return std::numeric_limits<double>::quiet_NaN();
			}

			if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
				return parseHexNumber(start, negative);
			}
		}

		std::string digits;
		if (negative) {
			digits += '-';
		}

		const size_t int_start = m_pos;
		while (isAsciiDigit(peek())) {
			digits += peek();
			m_pos++;
		}

		const size_t int_digits = m_pos - int_start;
		if (int_digits > 1 && m_text[int_start] == '0') {
			fail("leading zeros are not allowed", int_start);
		}

		if (int_digits == 0 && !(m_json5 && peek() == '.' && isAsciiDigit(peek(1)))) {
			fail("expected a digit");
		}

		bool is_float = false;

		if (peek() == '.') {
			is_float = true;
			digits += '.';
			m_pos++;

			const size_t frac_start = m_pos;
			while (isAsciiDigit(peek())) {
				digits += peek();
				m_pos++;
			}

			// JSON5 allows a trailing decimal point, like `5.`
			if (m_pos == frac_start && !m_json5) {
				fail("expected a digit");
			}
		}

		if (peek() == 'e' || peek() == 'E') {
			is_float = true;
			digits += 'e';
			m_pos++;

			if (peek() == '+' || peek() == '-') {
				digits += peek();
				m_pos++;
			}

			if (!isAsciiDigit(peek())) {
				fail("expected a digit");
			}

			while (isAsciiDigit(peek())) {
				digits += peek();
				m_pos++;
			}
		}

		if (!is_float) {
			const char *first = digits.data();
			const char *last = digits.data() + digits.size();

			int64_t value = 0;
			auto [ptr, ec] = std::from_chars(first, last, value);
			if (ec == std::errc()) {
				// Integers can't keep the sign of `-0`
				return negative && value == 0 ? Value(-0.0) : Value(value);
			}
			// Too large for an integer, fall back to floating-point
		}

		double value = 0;
		if (!tk::detail::parseDecimalFloat(digits, value)) {
			fail("number is out of range", start);
		}

		return value;
	}

	Value parseHexNumber(size_t start, bool negative)
	{
		m_pos += 2;

		if (!isAsciiHexDigit(peek())) {
			fail("expected a hexadecimal digit");
		}

		const size_t digits_start = m_pos;
		uint64_t value = 0;
		bool overflow = false;

		while (isAsciiHexDigit(peek())) {
			if (value > (UINT64_MAX >> 4)) {
				overflow = true;
			}

			value = (value << 4) | hexDigitValue(peek());
			m_pos++;
		}

		const uint64_t int_limit = negative ? uint64_t(INT64_MAX) + 1 : uint64_t(INT64_MAX);
		if (!overflow && value <= int_limit) {
			if (negative && value == 0) {
				// Integers can't keep the sign of `-0`
				return -0.0;
			}

			return negative ? int64_t(0 - value) : int64_t(value);
		}

		// Too large for an integer, fall back to floating-point. Parsing hex float
		// digits (without `0x` and exponent) gives a correctly rounded value.
		double float_value = 0;
		auto [ptr, ec] = std::from_chars(m_text.data() + digits_start, m_text.data() + m_pos, float_value,
			std::chars_format::hex);
		if (ec != std::errc()) {
			fail("number is out of range", start);
		}

		return negative ? -float_value : float_value;
	}

	uint32_t parseHexDigits(int count, size_t escape_start)
	{
		uint32_t value = 0;
		for (int i = 0; i < count; i++) {
			if (!isAsciiHexDigit(peek())) {
				fail("invalid escape sequence", escape_start);
			}
			value = (value << 4) | hexDigitValue(peek());
			m_pos++;
		}
		return value;
	}

	// Parses `XXXX` after `\u`, combining surrogate pairs
	char32_t parseUnicodeEscape(size_t escape_start)
	{
		const uint32_t value = parseHexDigits(4, escape_start);

		if (value >= 0xD800 && value <= 0xDBFF) {
			// High surrogate, must be followed by a low one
			if (!lookingAt("\\u")) {
				fail("unpaired surrogate in escape sequence", escape_start);
			}

			const size_t low_start = m_pos;
			m_pos += 2;
			const uint32_t low = parseHexDigits(4, low_start);

			if (low < 0xDC00 || low > 0xDFFF) {
				fail("unpaired surrogate in escape sequence", escape_start);
			}

			return char32_t(0x10000 + ((value - 0xD800) << 10) + (low - 0xDC00));
		}

		if (!isUnicodeScalar(char32_t(value))) {
			fail("unpaired surrogate in escape sequence", escape_start);
		}

		return char32_t(value);
	}

	// Parses escape sequence after backslash, appends the result to `out`
	void parseEscape(std::string &out)
	{
		const size_t start = m_pos - 1;

		if (atEnd()) {
			fail("unterminated string");
		}

		const char c = peek();
		m_pos++;

		switch (c) {
		case '"':
		case '\\':
		case '/':
			out += c;
			return;
		case 'b':
			out += '\b';
			return;
		case 'f':
			out += '\f';
			return;
		case 'n':
			out += '\n';
			return;
		case 'r':
			out += '\r';
			return;
		case 't':
			out += '\t';
			return;
		case 'u':
			appendUtf8(out, parseUnicodeEscape(start));
			return;
		default:
			break;
		}

		if (!m_json5) {
			fail("invalid escape sequence", start);
		}

		switch (c) {
		case '\'':
			out += '\'';
			return;
		case 'v':
			out += '\v';
			return;
		case '0':
			if (isAsciiDigit(peek())) {
				fail("invalid escape sequence", start);
			}
			out += '\0';
			return;
		case 'x':
			appendUtf8(out, char32_t(parseHexDigits(2, start)));
			return;
		case '\r':
			// Line continuation, `\r\n` counts as one line terminator
			if (peek() == '\n') {
				m_pos++;
			}
			return;
		case '\n':
			return;
		default:
			break;
		}

		if (isAsciiDigit(c)) {
			fail("invalid escape sequence", start);
		}

		// Other characters (including multi-byte ones) escape to themselves,
		// except U+2028/U+2029 which are line continuations
		m_pos--;
		size_t length = 0;
		const char32_t cp = peekCodePoint(length);
		if (cp != 0x2028 && cp != 0x2029) {
			out.append(m_text.substr(m_pos, length));
		}
		m_pos += length;
	}

	std::string parseString()
	{
		const size_t start = m_pos;
		const char quote = peek();
		m_pos++;

		std::string result;
		while (true) {
			if (atEnd()) {
				fail("unterminated string", start);
			}

			const char c = peek();

			if (c == quote) {
				m_pos++;
				return result;
			}

			if (c == '\\') {
				m_pos++;
				parseEscape(result);
				continue;
			}

			if (c == '\n' || c == '\r') {
				fail("unescaped line break in string");
			}

			// JSON5 allows raw control characters (except line terminators)
			if (!m_json5 && static_cast<unsigned char>(c) < 0x20) {
				fail("unescaped control character in string");
			}

			result += c;
			m_pos++;
		}
	}

	static bool isIdentifierStart(char c) noexcept
	{
		// All non-ASCII code points are accepted as well, see `parseIdentifier`
		return isAsciiAlpha(c) || c == '$' || c == '_' || static_cast<unsigned char>(c) >= 0x80;
	}

	// JSON5 unquoted keys. Strictly speaking, ECMAScript allows only Unicode
	// letters and some other categories here, but we accept any non-ASCII
	// code points (except whitespace) to avoid shipping Unicode tables.
	std::string parseIdentifier()
	{
		std::string result;

		while (!atEnd()) {
			const char c = peek();

			if (c == '\\') {
				const size_t escape_start = m_pos;
				if (peek(1) != 'u') {
					fail("invalid escape sequence in identifier", escape_start);
				}
				m_pos += 2;
				appendUtf8(result, parseUnicodeEscape(escape_start));
				continue;
			}

			if (isAsciiAlpha(c) || c == '$' || c == '_' || (isAsciiDigit(c) && !result.empty())) {
				result += c;
				m_pos++;
				continue;
			}

			if (static_cast<unsigned char>(c) >= 0x80) {
				size_t length = 0;
				if (isJson5Space(peekCodePoint(length))) {
					break;
				}
				result.append(m_text.substr(m_pos, length));
				m_pos += length;
				continue;
			}

			break;
		}

		return result;
	}

	Value parseArray()
	{
		enterNested();
		m_pos++;

		Array array;

		skipWhitespace();
		if (peek() == ']') {
			m_pos++;
			m_depth--;
			return array;
		}

		while (true) {
			array.push_back(parseValue());
			skipWhitespace();

			if (peek() == ',') {
				m_pos++;
				skipWhitespace();

				if (m_json5 && peek() == ']') {
					m_pos++;
					break;
				}
				continue;
			}

			if (peek() == ']') {
				m_pos++;
				break;
			}

			fail("expected ',' or ']' in array");
		}

		m_depth--;
		return array;
	}

	Value parseObject()
	{
		enterNested();
		m_pos++;

		Object object;
		std::optional<std::unordered_set<std::string>> hashed_keys;

		skipWhitespace();
		if (peek() == '}') {
			m_pos++;
			m_depth--;
			return object;
		}

		while (true) {
			const size_t key_start = m_pos;
			std::string key;

			if (peek() == '"' || (m_json5 && peek() == '\'')) {
				key = parseString();
			} else if (m_json5 && (isIdentifierStart(peek()) || peek() == '\\')) {
				key = parseIdentifier();
			}

			if (m_pos == key_start) {
				fail("expected a key");
			}

			skipWhitespace();
			if (peek() != ':') {
				fail("expected ':' after a key");
			}
			m_pos++;
			skipWhitespace();

			Value value = parseValue();

			const bool duplicate = hashed_keys ? !hashed_keys->insert(key).second : object.contains(key);
			if (duplicate) {
				fail("duplicate key '" + key + "'", key_start);
			}

			// Skip `insertOrAssign()`, its lookup is already done
			object.m_items.emplace_back(std::move(key), std::move(value));

			if (!hashed_keys && object.size() == HASHED_KEYS_THRESHOLD) {
				hashed_keys.emplace();
				for (const Object::Item &item : object) {
					hashed_keys->insert(item.first);
				}
			}

			skipWhitespace();

			if (peek() == ',') {
				m_pos++;
				skipWhitespace();

				if (m_json5 && peek() == '}') {
					m_pos++;
					break;
				}
				continue;
			}

			if (peek() == '}') {
				m_pos++;
				break;
			}

			fail("expected ',' or '}' in object");
		}

		m_depth--;
		return object;
	}
};

} // namespace detail

std::expected<Value, ParseError> parse(std::string_view text, Dialect dialect)
{
	return detail::Parser(text, dialect).run();
}

} // namespace tk::json
