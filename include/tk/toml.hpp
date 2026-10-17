#pragma once

#include <tk/parse_error.hpp>

#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

// TOML 1.0.0 parser and writer, https://toml.io/en/v1.0.0

namespace tk::toml
{

namespace detail
{

class Parser;

}

struct LocalDate {
	int32_t year = 0;
	uint8_t month = 1;
	uint8_t day = 1;

	auto operator<=>(const LocalDate &) const = default;
};

struct LocalTime {
	uint8_t hour = 0;
	uint8_t minute = 0;
	uint8_t second = 0;
	// Fractional seconds beyond nanosecond precision are truncated
	uint32_t nanosecond = 0;

	auto operator<=>(const LocalTime &) const = default;
};

struct LocalDateTime {
	LocalDate date;
	LocalTime time;

	auto operator<=>(const LocalDateTime &) const = default;
};

struct OffsetDateTime {
	LocalDate date;
	LocalTime time;
	// Offset from UTC in minutes, zero for `Z`
	int16_t offset_minutes = 0;

	// NOTE: compares fields, not moments in time
	auto operator<=>(const OffsetDateTime &) const = default;
};

class Value;

class Array {
public:
	Array() = default;
	Array(std::initializer_list<Value> values);

	using iterator = std::vector<Value>::iterator;
	using const_iterator = std::vector<Value>::const_iterator;

	size_t size() const noexcept;
	bool empty() const noexcept;

	Value &operator[](size_t index) noexcept;
	const Value &operator[](size_t index) const noexcept;

	iterator begin() noexcept;
	const_iterator begin() const noexcept;
	iterator end() noexcept;
	const_iterator end() const noexcept;

	Value &push_back(Value value);

	bool operator==(const Array &other) const noexcept;

private:
	std::vector<Value> m_values;
	// Parser state, see `detail::Parser`
	uint8_t m_flags = 0;

	friend class detail::Parser;
};

// Keys are kept in insertion order. Lookup is linear, which is
// fine for config-sized tables but not for thousands of keys.
class Table {
public:
	using Item = std::pair<std::string, Value>;

	Table() = default;
	Table(std::initializer_list<Item> items);

	using iterator = std::vector<Item>::iterator;
	using const_iterator = std::vector<Item>::const_iterator;

	size_t size() const noexcept;
	bool empty() const noexcept;

	iterator begin() noexcept;
	const_iterator begin() const noexcept;
	iterator end() noexcept;
	const_iterator end() const noexcept;

	// Returns null if there is no such key
	Value *find(std::string_view key) noexcept;
	const Value *find(std::string_view key) const noexcept;
	bool contains(std::string_view key) const noexcept;

	// Inserts a new key or replaces the value of existing one
	Value &insertOrAssign(std::string key, Value value);

	bool operator==(const Table &other) const noexcept;

private:
	std::vector<Item> m_items;
	// Parser state, see `detail::Parser`
	uint8_t m_flags = 0;

	friend class detail::Parser;
};

class Value {
public:
	// Alternatives are in the same order as `Type` values
	using Variant = std::variant<std::string, int64_t, double, bool, OffsetDateTime, LocalDateTime, LocalDate, LocalTime,
		Array, Table>;

	enum class Type : uint8_t {
		String,
		Integer,
		Float,
		Boolean,
		OffsetDateTime,
		LocalDateTime,
		LocalDate,
		LocalTime,
		Array,
		Table,
	};

	// Empty table
	Value() = default;

	Value(std::string value) noexcept : m_data(std::move(value)) {}
	Value(std::string_view value) : m_data(std::string(value)) {}
	Value(const char *value) : m_data(std::string(value)) {}

	template<std::integral T>
		requires(!std::same_as<T, bool> && !std::same_as<T, char>)
	Value(T value) noexcept : m_data(int64_t(value))
	{
		static_assert(sizeof(T) < sizeof(int64_t) || std::signed_integral<T>, "uint64_t might not fit");
	}

	// Would silently convert to `bool` otherwise
	Value(char) = delete;

	template<std::floating_point T>
	Value(T value) noexcept : m_data(double(value))
	{}

	Value(bool value) noexcept : m_data(value) {}
	Value(OffsetDateTime value) noexcept : m_data(value) {}
	Value(LocalDateTime value) noexcept : m_data(value) {}
	Value(LocalDate value) noexcept : m_data(value) {}
	Value(LocalTime value) noexcept : m_data(value) {}
	Value(Array value) noexcept : m_data(std::move(value)) {}
	Value(Table value) noexcept : m_data(std::move(value)) {}

	Type type() const noexcept { return Type(m_data.index()); }

	// Returns null if the value has another type
	template<typename T>
	T *get() noexcept
	{
		return std::get_if<T>(&m_data);
	}

	// Returns null if the value has another type
	template<typename T>
	const T *get() const noexcept
	{
		return std::get_if<T>(&m_data);
	}

	const Variant &variant() const noexcept { return m_data; }

	bool operator==(const Value &other) const noexcept = default;

private:
	Variant m_data { Table() };
};

// These need `Value` to be a complete type

inline size_t Array::size() const noexcept
{
	return m_values.size();
}

inline bool Array::empty() const noexcept
{
	return m_values.empty();
}

inline Value &Array::operator[](size_t index) noexcept
{
	return m_values[index];
}

inline const Value &Array::operator[](size_t index) const noexcept
{
	return m_values[index];
}

inline auto Array::begin() noexcept -> iterator
{
	return m_values.begin();
}

inline auto Array::begin() const noexcept -> const_iterator
{
	return m_values.begin();
}

inline auto Array::end() noexcept -> iterator
{
	return m_values.end();
}

inline auto Array::end() const noexcept -> const_iterator
{
	return m_values.end();
}

inline size_t Table::size() const noexcept
{
	return m_items.size();
}

inline bool Table::empty() const noexcept
{
	return m_items.empty();
}

inline auto Table::begin() noexcept -> iterator
{
	return m_items.begin();
}

inline auto Table::begin() const noexcept -> const_iterator
{
	return m_items.begin();
}

inline auto Table::end() noexcept -> iterator
{
	return m_items.end();
}

inline auto Table::end() const noexcept -> const_iterator
{
	return m_items.end();
}

inline bool Table::contains(std::string_view key) const noexcept
{
	return find(key) != nullptr;
}

// Parses a TOML document. Text must be valid UTF-8.
std::expected<Table, ParseError> parse(std::string_view text);

// Formats a key, quoting it if it can't be bare
std::string formatKey(std::string_view key);
// Formats a value as it would appear after `key = `. Tables are written inline.
// Dates and times must be valid (as produced by `parse()`), this is only asserted.
std::string formatValue(const Value &value);
// Formats a complete document. Sub-tables are written as `[table]` sections,
// arrays of tables as `[[array]]` sections. Comments and original formatting
// are not preserved (there is nowhere to keep them in the DOM).
std::string serialize(const Table &table);

} // namespace tk::toml
