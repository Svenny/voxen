module;

#include <algorithm>
#include <cassert>
#include <charconv>
#include <cmath>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

module tk.toml;

import tk.text_utils;

namespace tk::toml
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

Table::Table(std::initializer_list<Item> items)
{
	for (const Item &item : items) {
		insertOrAssign(item.first, item.second);
	}
}

Value *Table::find(std::string_view key) noexcept
{
	for (Item &item : m_items) {
		if (item.first == key) {
			return &item.second;
		}
	}

	return nullptr;
}

const Value *Table::find(std::string_view key) const noexcept
{
	return const_cast<Table *>(this)->find(key);
}

Value &Table::insertOrAssign(std::string key, Value value)
{
	if (Value *existing = find(key); existing) {
		*existing = std::move(value);
		return *existing;
	}

	return m_items.emplace_back(std::move(key), std::move(value)).second;
}

bool Table::operator==(const Table &other) const noexcept
{
	// Key order does not matter
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

namespace detail
{

using tk::detail::appendUtf8;
using tk::detail::hexDigitValue;
using tk::detail::isAsciiAlpha;
using tk::detail::isAsciiDigit;
using tk::detail::isAsciiHexDigit;
using tk::detail::isUnicodeScalar;

class Parser {
public:
	explicit Parser(std::string_view text) noexcept : m_text(text) {}

	std::expected<Table, ParseError> run()
	{
		if (size_t bad = tk::detail::findInvalidUtf8(m_text); bad != std::string_view::npos) {
			return std::unexpected(tk::detail::makeParseError(m_text, bad, "invalid UTF-8"));
		}

		try {
			return parseDocument();
		}
		catch (Failure &failure) {
			return std::unexpected(tk::detail::makeParseError(m_text, failure.offset, std::move(failure.message)));
		}
	}

private:
	// Table defined by `[header]` (or by `[[header]]` as an array element)
	constexpr static uint8_t TABLE_EXPLICIT = 1;
	// Table defined by dotted keys (`a.b = 1`)
	constexpr static uint8_t TABLE_DOTTED = 2;
	// Inline table (`a = { b = 1 }`) or its sub-table, can't be modified
	constexpr static uint8_t TABLE_INLINE = 4;
	// Tables without flags were created implicitly as parents of `[a.b]` headers

	// Array defined by `[[header]]`, can be appended to by more headers
	constexpr static uint8_t ARRAY_OF_TABLES = 1;

	// Limit for nesting of tables (including dotted keys and headers) and arrays.
	// The parser and DOM operations (destruction, comparison etc.) recurse that deep.
	constexpr static int32_t MAX_NESTING_DEPTH = 128;

	struct Failure {
		size_t offset;
		std::string message;
	};

	struct KeyPart {
		std::string name;
		size_t offset;
	};

	std::string_view m_text;
	size_t m_pos = 0;
	// Nesting depth of the value being parsed
	int32_t m_depth = 0;

	[[noreturn]] void fail(std::string message, size_t offset) { throw Failure { offset, std::move(message) }; }
	[[noreturn]] void fail(std::string message) { fail(std::move(message), m_pos); }

	bool atEnd() const noexcept { return m_pos >= m_text.size(); }

	// Returns '\0' past the end of text
	char peek(size_t ahead = 0) const noexcept { return m_pos + ahead < m_text.size() ? m_text[m_pos + ahead] : '\0'; }

	bool lookingAt(std::string_view s) const noexcept { return m_text.substr(m_pos).starts_with(s); }

	void expect(char c, const char *what)
	{
		if (peek() != c || atEnd()) {
			fail(std::string("expected ") + what);
		}
		m_pos++;
	}

	// Control characters are not allowed in comments and strings (except tab)
	static bool isControl(char c) noexcept { return (c >= 0 && c < 0x20 && c != '\t') || c == 0x7F; }

	static bool isBareKeyChar(char c) noexcept { return isAsciiAlpha(c) || isAsciiDigit(c) || c == '_' || c == '-'; }

	void skipWs() noexcept
	{
		while (peek() == ' ' || peek() == '\t') {
			m_pos++;
		}
	}

	void skipComment()
	{
		if (peek() != '#') {
			return;
		}

		m_pos++;
		while (!atEnd() && peek() != '\n') {
			if (peek() == '\r' && peek(1) == '\n') {
				break;
			}

			if (isControl(peek())) {
				fail("control character in comment");
			}

			m_pos++;
		}
	}

	bool tryConsumeNewline() noexcept
	{
		if (peek() == '\n') {
			m_pos++;
			return true;
		}

		if (peek() == '\r' && peek(1) == '\n') {
			m_pos += 2;
			return true;
		}

		return false;
	}

	// Skip whitespace, comments and newlines (allowed inside arrays)
	void skipWsCommentsNewlines()
	{
		while (true) {
			skipWs();
			skipComment();
			if (!tryConsumeNewline()) {
				return;
			}
		}
	}

	void expectLineEnd()
	{
		skipWs();
		skipComment();

		if (!atEnd() && !tryConsumeNewline()) {
			fail("expected end of line");
		}
	}

	Table parseDocument()
	{
		// Skip UTF-8 BOM if present
		if (lookingAt("\xEF\xBB\xBF")) {
			m_pos += 3;
		}

		Table root;
		Table *current = &root;

		while (true) {
			skipWs();
			if (atEnd()) {
				break;
			}

			if (peek() == '[') {
				current = parseTableHeader(root);
			} else if (peek() != '#' && peek() != '\n' && peek() != '\r') {
				parseKeyValue(*current);
			}

			expectLineEnd();
		}

		return root;
	}

	std::vector<KeyPart> parseKey()
	{
		std::vector<KeyPart> parts;

		while (true) {
			skipWs();

			KeyPart &part = parts.emplace_back(KeyPart { .name = {}, .offset = m_pos });

			if (peek() == '"') {
				if (lookingAt("\"\"\"")) {
					fail("multi-line string can't be a key");
				}
				part.name = parseBasicString();
			} else if (peek() == '\'') {
				if (lookingAt("'''")) {
					fail("multi-line string can't be a key");
				}
				part.name = parseLiteralString();
			} else {
				const size_t start = m_pos;
				while (isBareKeyChar(peek())) {
					m_pos++;
				}

				if (m_pos == start) {
					fail("expected a key");
				}

				part.name = std::string(m_text.substr(start, m_pos - start));
			}

			skipWs();
			if (peek() != '.') {
				return parts;
			}
			m_pos++;
		}
	}

	static std::string describeKey(const std::vector<KeyPart> &key, size_t count)
	{
		std::string result;
		for (size_t i = 0; i < count; i++) {
			if (i > 0) {
				result += '.';
			}
			result += formatKey(key[i].name);
		}
		return result;
	}

	static Table &insertTable(Table &parent, std::string name, uint8_t flags)
	{
		Table &table = *parent.m_items.emplace_back(std::move(name), Table()).second.get<Table>();
		table.m_flags = flags;
		return table;
	}

	void parseKeyValue(Table &target)
	{
		const std::vector<KeyPart> key = parseKey();

		expect('=', "'=' after a key");
		skipWs();

		// Each dotted key part adds a table level
		const size_t dotted_depth = key.size() - 1;
		if (dotted_depth > size_t(MAX_NESTING_DEPTH - m_depth)) {
			fail("nesting is too deep", key.front().offset);
		}

		m_depth += int32_t(dotted_depth);
		Value value = parseValue();
		m_depth -= int32_t(dotted_depth);

		Table *table = &target;
		for (size_t i = 0; i < key.size() - 1; i++) {
			Value *existing = table->find(key[i].name);

			if (!existing) {
				table = &insertTable(*table, key[i].name, TABLE_DOTTED);
				continue;
			}

			Table *sub_table = existing->get<Table>();
			if (!sub_table || (sub_table->m_flags & TABLE_DOTTED) == 0 || (sub_table->m_flags & TABLE_INLINE) != 0) {
				fail("can't add keys to '" + describeKey(key, i + 1) + "', it is already defined", key[i].offset);
			}

			table = sub_table;
		}

		if (table->find(key.back().name)) {
			fail("duplicate key '" + describeKey(key, key.size()) + "'", key.back().offset);
		}

		table->m_items.emplace_back(key.back().name, std::move(value));
	}

	Table *parseTableHeader(Table &root)
	{
		const bool is_array = lookingAt("[[");
		m_pos += is_array ? 2 : 1;

		const std::vector<KeyPart> key = parseKey();

		if (is_array) {
			if (!lookingAt("]]")) {
				fail("expected ']]' after array of tables name");
			}
			m_pos += 2;
		} else {
			expect(']', "']' after table name");
		}

		// Key-values after the header start at the depth of its table
		m_depth = 0;
		auto descend = [&](int32_t levels) {
			m_depth += levels;
			if (m_depth > MAX_NESTING_DEPTH) {
				fail("nesting is too deep", key.front().offset);
			}
		};

		// Array of tables counts as two levels (array and its element)
		descend(is_array ? 2 : 1);

		Table *table = &root;
		for (size_t i = 0; i < key.size() - 1; i++) {
			descend(1);
			Value *existing = table->find(key[i].name);

			if (!existing) {
				table = &insertTable(*table, key[i].name, 0);
				continue;
			}

			if (Table *sub_table = existing->get<Table>(); sub_table) {
				if (sub_table->m_flags & TABLE_INLINE) {
					fail("can't extend inline table '" + describeKey(key, i + 1) + "'", key[i].offset);
				}

				table = sub_table;
				continue;
			}

			if (Array *array = existing->get<Array>(); array && (array->m_flags & ARRAY_OF_TABLES)) {
				// Refers to the last defined element, one more level
				descend(1);
				table = array->m_values.back().get<Table>();
				continue;
			}

			fail("'" + describeKey(key, i + 1) + "' is already defined as a non-table value", key[i].offset);
		}

		const KeyPart &last = key.back();
		Value *existing = table->find(last.name);

		if (!is_array) {
			if (!existing) {
				return &insertTable(*table, last.name, TABLE_EXPLICIT);
			}

			Table *sub_table = existing->get<Table>();
			if (!sub_table || (sub_table->m_flags & (TABLE_EXPLICIT | TABLE_DOTTED | TABLE_INLINE)) != 0) {
				fail("'" + describeKey(key, key.size()) + "' is already defined", last.offset);
			}

			sub_table->m_flags |= TABLE_EXPLICIT;
			return sub_table;
		}

		Array *array = nullptr;
		if (!existing) {
			array = table->m_items.emplace_back(last.name, Array()).second.get<Array>();
			array->m_flags = ARRAY_OF_TABLES;
		} else {
			array = existing->get<Array>();
			if (!array || (array->m_flags & ARRAY_OF_TABLES) == 0) {
				fail("'" + describeKey(key, key.size()) + "' is already defined and is not an array of tables",
					last.offset);
			}
		}

		Table *element = array->push_back(Table()).get<Table>();
		element->m_flags = TABLE_EXPLICIT;
		return element;
	}

	Value parseValue()
	{
		const char c = peek();

		if (atEnd()) {
			fail("expected a value");
		}

		if (c == '"') {
			return lookingAt("\"\"\"") ? parseMultilineBasicString() : parseBasicString();
		}

		if (c == '\'') {
			return lookingAt("'''") ? parseMultilineLiteralString() : parseLiteralString();
		}

		if (c == '[') {
			return parseArray();
		}

		if (c == '{') {
			return parseInlineTable();
		}

		if (lookingAt("true")) {
			m_pos += 4;
			return true;
		}

		if (lookingAt("false")) {
			m_pos += 5;
			return false;
		}

		if (isAsciiDigit(c) && isAsciiDigit(peek(1))) {
			if (isAsciiDigit(peek(2)) && isAsciiDigit(peek(3)) && peek(4) == '-') {
				return parseDateTime();
			}

			if (peek(2) == ':') {
				return parseTime();
			}
		}

		if (isAsciiDigit(c) || c == '+' || c == '-' || c == 'i' || c == 'n') {
			return parseNumber();
		}

		fail("expected a value");
	}

	// Parses an escape sequence after backslash, appends the result to `out`
	void parseEscape(std::string &out)
	{
		const size_t start = m_pos - 1;
		const char c = peek();
		m_pos++;

		switch (c) {
		case 'b':
			out += '\b';
			return;
		case 't':
			out += '\t';
			return;
		case 'n':
			out += '\n';
			return;
		case 'f':
			out += '\f';
			return;
		case 'r':
			out += '\r';
			return;
		case '"':
			out += '"';
			return;
		case '\\':
			out += '\\';
			return;
		case 'u':
		case 'U': {
			const int digits = c == 'u' ? 4 : 8;
			uint32_t value = 0;

			for (int i = 0; i < digits; i++) {
				if (!isAsciiHexDigit(peek())) {
					fail("invalid Unicode escape", start);
				}
				value = (value << 4) | hexDigitValue(peek());
				m_pos++;
			}

			if (!isUnicodeScalar(char32_t(value))) {
				fail("escape is not a valid Unicode scalar value", start);
			}

			appendUtf8(out, char32_t(value));
			return;
		}
		default:
			fail("invalid escape sequence", start);
		}
	}

	std::string parseBasicString()
	{
		const size_t start = m_pos;
		m_pos++;

		std::string result;
		while (true) {
			if (atEnd() || peek() == '\n' || (peek() == '\r' && peek(1) == '\n')) {
				fail("unterminated string", start);
			}

			const char c = peek();
			if (c == '"') {
				m_pos++;
				return result;
			}

			if (c == '\\') {
				m_pos++;
				parseEscape(result);
				continue;
			}

			if (isControl(c)) {
				fail("control character in string");
			}

			result += c;
			m_pos++;
		}
	}

	std::string parseLiteralString()
	{
		const size_t start = m_pos;
		m_pos++;

		while (true) {
			if (atEnd() || peek() == '\n' || (peek() == '\r' && peek(1) == '\n')) {
				fail("unterminated string", start);
			}

			const char c = peek();
			if (c == '\'') {
				m_pos++;
				return std::string(m_text.substr(start + 1, m_pos - start - 2));
			}

			if (isControl(c)) {
				fail("control character in string");
			}

			m_pos++;
		}
	}

	// Handles closing delimiter of multi-line strings. Returns true if the string has ended.
	// Up to two quotes right before the delimiter are part of the string content.
	bool tryCloseMultiline(char quote, std::string &out)
	{
		size_t count = 0;
		while (peek(count) == quote) {
			count++;
		}

		if (count < 3) {
			out.append(count, quote);
			m_pos += count;
			return false;
		}

		if (count > 5) {
			fail("too many quotes in multi-line string", m_pos + 5);
		}

		out.append(count - 3, quote);
		m_pos += count;
		return true;
	}

	std::string parseMultilineBasicString()
	{
		const size_t start = m_pos;
		m_pos += 3;
		// Newline right after the delimiter is trimmed
		tryConsumeNewline();

		std::string result;
		while (true) {
			if (atEnd()) {
				fail("unterminated string", start);
			}

			const char c = peek();
			if (c == '"') {
				if (tryCloseMultiline('"', result)) {
					return result;
				}
				continue;
			}

			if (c == '\\') {
				m_pos++;

				// Line ending backslash trims all whitespace and newlines after it
				size_t ws_end = m_pos;
				while (ws_end < m_text.size() && (m_text[ws_end] == ' ' || m_text[ws_end] == '\t')) {
					ws_end++;
				}

				const std::string_view rest = m_text.substr(ws_end);
				if (rest.starts_with('\n') || rest.starts_with("\r\n")) {
					m_pos = ws_end;
					while (true) {
						skipWs();
						if (!tryConsumeNewline()) {
							break;
						}
					}
					continue;
				}

				parseEscape(result);
				continue;
			}

			if (tryConsumeNewline()) {
				result += '\n';
				continue;
			}

			if (isControl(c)) {
				fail("control character in string");
			}

			result += c;
			m_pos++;
		}
	}

	std::string parseMultilineLiteralString()
	{
		const size_t start = m_pos;
		m_pos += 3;
		// Newline right after the delimiter is trimmed
		tryConsumeNewline();

		std::string result;
		while (true) {
			if (atEnd()) {
				fail("unterminated string", start);
			}

			const char c = peek();
			if (c == '\'') {
				if (tryCloseMultiline('\'', result)) {
					return result;
				}
				continue;
			}

			if (tryConsumeNewline()) {
				result += '\n';
				continue;
			}

			if (isControl(c)) {
				fail("control character in string");
			}

			result += c;
			m_pos++;
		}
	}

	// Scans digits with optional single underscores between them, appends digits to `out`
	template<typename Pred>
	void scanDigits(Pred is_digit, std::string &out)
	{
		if (!is_digit(peek())) {
			fail("expected a digit");
		}

		while (true) {
			if (is_digit(peek())) {
				out += peek();
				m_pos++;
			} else if (peek() == '_') {
				if (!is_digit(peek(1))) {
					fail("underscore must be between digits");
				}
				m_pos++;
			} else {
				return;
			}
		}
	}

	Value parseNumber()
	{
		const size_t start = m_pos;
		std::string digits;

		const char sign = peek();
		if (sign == '+' || sign == '-') {
			if (sign == '-') {
				digits += '-';
			}
			m_pos++;
		}

		if (lookingAt("inf") || lookingAt("nan")) {
			const bool is_nan = peek() == 'n';
			m_pos += 3;

			double value = is_nan ? std::numeric_limits<double>::quiet_NaN() : std::numeric_limits<double>::infinity();
			return sign == '-' ? -value : value;
		}

		if (sign != '+' && sign != '-' && peek() == '0' && (peek(1) == 'x' || peek(1) == 'o' || peek(1) == 'b')) {
			return parsePrefixedInteger();
		}

		if (!isAsciiDigit(peek())) {
			fail("expected a value", start);
		}

		const size_t int_start = m_pos;
		scanDigits(isAsciiDigit, digits);

		if (m_text[int_start] == '0' && m_pos - int_start > 1) {
			fail("leading zeros are not allowed", int_start);
		}

		bool is_float = false;

		if (peek() == '.') {
			is_float = true;
			digits += '.';
			m_pos++;
			scanDigits(isAsciiDigit, digits);
		}

		if (peek() == 'e' || peek() == 'E') {
			is_float = true;
			digits += 'e';
			m_pos++;

			if (peek() == '+' || peek() == '-') {
				digits += peek();
				m_pos++;
			}

			scanDigits(isAsciiDigit, digits);
		}

		const char *first = digits.data();
		const char *last = digits.data() + digits.size();

		if (is_float) {
			double value = 0;
			if (!tk::detail::parseDecimalFloat(digits, value)) {
				fail("float is out of range", start);
			}
			return value;
		}

		int64_t value = 0;
		auto [ptr, ec] = std::from_chars(first, last, value);
		if (ec == std::errc::result_out_of_range) {
			fail("integer is out of range", start);
		}
		return value;
	}

	Value parsePrefixedInteger()
	{
		const size_t start = m_pos;
		const char prefix = peek(1);
		m_pos += 2;

		uint32_t base = 16;
		bool (*is_digit)(char) = isAsciiHexDigit;

		if (prefix == 'o') {
			base = 8;
			is_digit = [](char c) { return c >= '0' && c <= '7'; };
		} else if (prefix == 'b') {
			base = 2;
			is_digit = [](char c) { return c == '0' || c == '1'; };
		}

		std::string digits;
		scanDigits(is_digit, digits);

		uint64_t value = 0;
		for (char c : digits) {
			const uint64_t digit = hexDigitValue(c);
			if (value > (uint64_t(INT64_MAX) - digit) / base) {
				fail("integer is out of range", start);
			}
			value = value * base + digit;
		}

		return int64_t(value);
	}

	uint32_t parseFixedDigits(int count)
	{
		uint32_t value = 0;
		for (int i = 0; i < count; i++) {
			if (!isAsciiDigit(peek())) {
				fail("invalid date/time format");
			}
			value = value * 10 + uint32_t(peek() - '0');
			m_pos++;
		}
		return value;
	}

	LocalDate parseDate()
	{
		const size_t start = m_pos;

		LocalDate date;
		date.year = int32_t(parseFixedDigits(4));
		expect('-', "'-' in date");
		const uint32_t month = parseFixedDigits(2);
		expect('-', "'-' in date");
		const uint32_t day = parseFixedDigits(2);

		constexpr uint32_t DAYS_IN_MONTH[] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
		if (month < 1 || month > 12 || day < 1 || day > DAYS_IN_MONTH[month - 1]) {
			fail("invalid date", start);
		}

		const bool is_leap = (date.year % 4 == 0 && date.year % 100 != 0) || date.year % 400 == 0;
		if (month == 2 && day == 29 && !is_leap) {
			fail("invalid date (not a leap year)", start);
		}

		date.month = uint8_t(month);
		date.day = uint8_t(day);
		return date;
	}

	LocalTime parseTimeOfDay()
	{
		const size_t start = m_pos;

		const uint32_t hour = parseFixedDigits(2);
		expect(':', "':' in time");
		const uint32_t minute = parseFixedDigits(2);
		expect(':', "':' in time");
		const uint32_t second = parseFixedDigits(2);

		// Allow leap seconds, as RFC 3339 does
		if (hour > 23 || minute > 59 || second > 60) {
			fail("invalid time", start);
		}

		LocalTime time { .hour = uint8_t(hour), .minute = uint8_t(minute), .second = uint8_t(second) };

		if (peek() == '.') {
			m_pos++;
			if (!isAsciiDigit(peek())) {
				fail("expected fractional seconds");
			}

			uint32_t scale = 100'000'000;
			while (isAsciiDigit(peek())) {
				time.nanosecond += uint32_t(peek() - '0') * scale;
				scale /= 10;
				m_pos++;
			}
		}

		return time;
	}

	Value parseTime() { return parseTimeOfDay(); }

	Value parseDateTime()
	{
		const LocalDate date = parseDate();

		const bool has_time = peek() == 'T' || peek() == 't'
			|| (peek() == ' ' && isAsciiDigit(peek(1)) && isAsciiDigit(peek(2)) && peek(3) == ':');
		if (!has_time) {
			return date;
		}

		m_pos++;
		const LocalTime time = parseTimeOfDay();

		if (peek() == 'Z' || peek() == 'z') {
			m_pos++;
			return OffsetDateTime { .date = date, .time = time, .offset_minutes = 0 };
		}

		if (peek() == '+' || peek() == '-') {
			const size_t start = m_pos;
			const bool negative = peek() == '-';
			m_pos++;

			const uint32_t hours = parseFixedDigits(2);
			expect(':', "':' in time offset");
			const uint32_t minutes = parseFixedDigits(2);

			if (hours > 23 || minutes > 59) {
				fail("invalid time offset", start);
			}

			const auto offset = int16_t(hours * 60 + minutes);
			return OffsetDateTime { .date = date, .time = time, .offset_minutes = negative ? int16_t(-offset) : offset };
		}

		return LocalDateTime { .date = date, .time = time };
	}

	void enterNested()
	{
		if (++m_depth > MAX_NESTING_DEPTH) {
			fail("nesting is too deep");
		}
	}

	Value parseArray()
	{
		enterNested();
		m_pos++;

		Array array;
		while (true) {
			skipWsCommentsNewlines();
			if (peek() == ']') {
				m_pos++;
				break;
			}

			array.push_back(parseValue());

			skipWsCommentsNewlines();
			if (peek() == ',') {
				m_pos++;
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

	static void markInline(Table &table) noexcept
	{
		table.m_flags |= TABLE_INLINE;

		// Arrays are always static here, only tables need marking
		for (auto &item : table.m_items) {
			if (Table *sub_table = item.second.get<Table>(); sub_table) {
				markInline(*sub_table);
			}
		}
	}

	Value parseInlineTable()
	{
		enterNested();
		m_pos++;

		Table table;

		skipWs();
		if (peek() == '}') {
			m_pos++;
		} else {
			while (true) {
				skipWs();
				parseKeyValue(table);
				skipWs();

				if (peek() == ',') {
					m_pos++;
					continue;
				}

				if (peek() == '}') {
					m_pos++;
					break;
				}

				fail("expected ',' or '}' in inline table");
			}
		}

		markInline(table);
		m_depth--;
		return table;
	}
};

} // namespace detail

std::expected<Table, ParseError> parse(std::string_view text)
{
	return detail::Parser(text).run();
}

namespace
{

void appendTwoDigits(std::string &out, uint32_t value)
{
	out += char('0' + value / 10 % 10);
	out += char('0' + value % 10);
}

void appendDate(std::string &out, const LocalDate &date)
{
	assert(date.year >= 0 && date.year <= 9999);
	assert(date.month >= 1 && date.month <= 12 && date.day >= 1 && date.day <= 31);

	const auto year = uint32_t(date.year);
	appendTwoDigits(out, year / 100);
	appendTwoDigits(out, year % 100);
	out += '-';
	appendTwoDigits(out, date.month);
	out += '-';
	appendTwoDigits(out, date.day);
}

void appendTime(std::string &out, const LocalTime &time)
{
	assert(time.hour <= 23 && time.minute <= 59 && time.second <= 60);
	assert(time.nanosecond < 1'000'000'000);

	appendTwoDigits(out, time.hour);
	out += ':';
	appendTwoDigits(out, time.minute);
	out += ':';
	appendTwoDigits(out, time.second);

	if (time.nanosecond != 0) {
		std::string fraction = std::to_string(time.nanosecond + 1'000'000'000).substr(1);
		while (fraction.back() == '0') {
			fraction.pop_back();
		}

		out += '.';
		out += fraction;
	}
}

void appendOffset(std::string &out, int16_t offset_minutes)
{
	if (offset_minutes == 0) {
		out += 'Z';
		return;
	}

	out += offset_minutes < 0 ? '-' : '+';
	const auto abs_offset = uint32_t(std::abs(offset_minutes));
	assert(abs_offset < 24 * 60);
	appendTwoDigits(out, abs_offset / 60);
	out += ':';
	appendTwoDigits(out, abs_offset % 60);
}

void appendString(std::string &out, std::string_view str)
{
	constexpr char HEX_DIGITS[] = "0123456789ABCDEF";

	out += '"';
	for (char c : str) {
		switch (c) {
		case '"':
			out += "\\\"";
			break;
		case '\\':
			out += "\\\\";
			break;
		case '\b':
			out += "\\b";
			break;
		case '\t':
			out += "\\t";
			break;
		case '\n':
			out += "\\n";
			break;
		case '\f':
			out += "\\f";
			break;
		case '\r':
			out += "\\r";
			break;
		default:
			if ((c >= 0 && c < 0x20) || c == 0x7F) {
				out += "\\u00";
				out += HEX_DIGITS[(c >> 4) & 0xF];
				out += HEX_DIGITS[c & 0xF];
			} else {
				out += c;
			}
		}
	}
	out += '"';
}

void appendFloat(std::string &out, double value)
{
	if (std::isnan(value)) {
		out += "nan";
		return;
	}

	if (std::isinf(value)) {
		out += value < 0 ? "-inf" : "inf";
		return;
	}

	char buffer[32];
	auto [ptr, ec] = std::to_chars(std::begin(buffer), std::end(buffer), value);
	const std::string_view text(buffer, ptr);
	out += text;

	// TOML floats need a fractional part or an exponent
	if (text.find_first_of(".e") == std::string_view::npos) {
		out += ".0";
	}
}

void appendValue(std::string &out, const Value &value);

void appendInlineTable(std::string &out, const Table &table)
{
	if (table.empty()) {
		out += "{}";
		return;
	}

	out += "{ ";
	bool first = true;
	for (const auto &[key, item] : table) {
		if (!first) {
			out += ", ";
		}
		first = false;

		out += formatKey(key);
		out += " = ";
		appendValue(out, item);
	}
	out += " }";
}

void appendValue(std::string &out, const Value &value)
{
	struct Visitor {
		std::string &out;

		void operator()(const std::string &v) { appendString(out, v); }
		void operator()(int64_t v) { out += std::to_string(v); }
		void operator()(double v) { appendFloat(out, v); }
		void operator()(bool v) { out += v ? "true" : "false"; }

		void operator()(const OffsetDateTime &v)
		{
			appendDate(out, v.date);
			out += 'T';
			appendTime(out, v.time);
			appendOffset(out, v.offset_minutes);
		}

		void operator()(const LocalDateTime &v)
		{
			appendDate(out, v.date);
			out += 'T';
			appendTime(out, v.time);
		}

		void operator()(const LocalDate &v) { appendDate(out, v); }
		void operator()(const LocalTime &v) { appendTime(out, v); }

		void operator()(const Array &v)
		{
			out += '[';
			bool first = true;
			for (const Value &item : v) {
				if (!first) {
					out += ", ";
				}
				first = false;
				appendValue(out, item);
			}
			out += ']';
		}

		void operator()(const Table &v) { appendInlineTable(out, v); }
	};

	std::visit(Visitor { out }, value.variant());
}

bool isArrayOfTables(const Value &value) noexcept
{
	const Array *array = value.get<Array>();
	if (!array || array->empty()) {
		return false;
	}

	return std::ranges::all_of(*array, [](const Value &item) { return item.type() == Value::Type::Table; });
}

bool isSection(const Value &value) noexcept
{
	return value.type() == Value::Type::Table || isArrayOfTables(value);
}

void appendSections(std::string &out, const Table &table, const std::string &path);

// Writes simple values of the table, then its sub-tables as sections
void appendTableBody(std::string &out, const Table &table, const std::string &path)
{
	for (const auto &[key, value] : table) {
		if (!isSection(value)) {
			out += formatKey(key);
			out += " = ";
			appendValue(out, value);
			out += '\n';
		}
	}

	appendSections(out, table, path);
}

void appendSections(std::string &out, const Table &table, const std::string &path)
{
	for (const auto &[key, value] : table) {
		if (!isSection(value)) {
			continue;
		}

		const std::string sub_path = path.empty() ? formatKey(key) : path + '.' + formatKey(key);

		if (const Table *sub_table = value.get<Table>(); sub_table) {
			// Tables with only sub-tables don't need their own header,
			// they will be defined implicitly by headers of sub-tables
			const bool has_simple_values = std::ranges::any_of(*sub_table,
				[](const auto &item) { return !isSection(item.second); });

			if (has_simple_values || sub_table->empty()) {
				if (!out.empty()) {
					out += '\n';
				}

				out += '[';
				out += sub_path;
				out += "]\n";
			}

			appendTableBody(out, *sub_table, sub_path);
			continue;
		}

		for (const Value &element : *value.get<Array>()) {
			if (!out.empty()) {
				out += '\n';
			}

			out += "[[";
			out += sub_path;
			out += "]]\n";
			appendTableBody(out, *element.get<Table>(), sub_path);
		}
	}
}

} // namespace

std::string formatKey(std::string_view key)
{
	const bool is_bare = !key.empty() && std::ranges::all_of(key, [](char c) {
		return tk::detail::isAsciiAlpha(c) || tk::detail::isAsciiDigit(c) || c == '_' || c == '-';
	});

	if (is_bare) {
		return std::string(key);
	}

	std::string result;
	appendString(result, key);
	return result;
}

std::string formatValue(const Value &value)
{
	std::string result;
	appendValue(result, value);
	return result;
}

std::string serialize(const Table &table)
{
	std::string result;
	appendTableBody(result, table, std::string());
	return result;
}

} // namespace tk::toml
