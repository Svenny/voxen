#pragma once

#include <tk/parse_error.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

// JSON (RFC 8259) and JSON5 (https://spec.json5.org) parser.
// Read-only: writing JSON is a separate task with different needs.

namespace tk::json
{

enum class Dialect : uint8_t {
	// Strict JSON as defined by RFC 8259
	Json,
	// JSON5: comments, trailing commas, unquoted keys, single-quoted strings,
	// hexadecimal numbers, `Infinity`/`NaN` and some more relaxations
	Json5,
};

namespace detail
{

class Parser;

}

class Value;

class Array {
public:
	using iterator = std::vector<Value>::iterator;
	using const_iterator = std::vector<Value>::const_iterator;

	Array() = default;
	Array(std::initializer_list<Value> values);

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
};

// Keys are kept in document order. Lookup is linear, which is fine for
// typical asset/config files but not for objects with thousands of keys.
class Object {
public:
	using Item = std::pair<std::string, Value>;
	using iterator = std::vector<Item>::iterator;
	using const_iterator = std::vector<Item>::const_iterator;

	Object() = default;
	Object(std::initializer_list<Item> items);

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

	// Key order does not matter
	bool operator==(const Object &other) const noexcept;

private:
	std::vector<Item> m_items;

	friend class detail::Parser;
};

class Value {
public:
	// Alternatives are in the same order as `Type` values
	using Variant = std::variant<std::nullptr_t, bool, int64_t, double, std::string, Array, Object>;

	enum class Type : uint8_t {
		Null,
		Boolean,
		// Numbers without fraction and exponent that fit into `int64_t`, except `-0`
		Integer,
		// All other numbers
		Float,
		String,
		Array,
		Object,
	};

	// Null value
	Value() = default;

	Value(std::nullptr_t) noexcept {}
	Value(bool value) noexcept : m_data(value) {}

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

	Value(std::string value) noexcept : m_data(std::move(value)) {}
	Value(std::string_view value) : m_data(std::string(value)) {}
	Value(const char *value) : m_data(std::string(value)) {}
	Value(Array value) noexcept : m_data(std::move(value)) {}
	Value(Object value) noexcept : m_data(std::move(value)) {}

	Type type() const noexcept { return Type(m_data.index()); }
	bool isNull() const noexcept { return type() == Type::Null; }

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

	// Numeric value of either `Integer` or `Float`, empty for other types
	std::optional<double> number() const noexcept;

	const Variant &variant() const noexcept { return m_data; }

	bool operator==(const Value &other) const noexcept = default;

private:
	Variant m_data;
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

inline size_t Object::size() const noexcept
{
	return m_items.size();
}

inline bool Object::empty() const noexcept
{
	return m_items.empty();
}

inline auto Object::begin() noexcept -> iterator
{
	return m_items.begin();
}

inline auto Object::begin() const noexcept -> const_iterator
{
	return m_items.begin();
}

inline auto Object::end() noexcept -> iterator
{
	return m_items.end();
}

inline auto Object::end() const noexcept -> const_iterator
{
	return m_items.end();
}

inline bool Object::contains(std::string_view key) const noexcept
{
	return find(key) != nullptr;
}

// Parses a document. Text must be valid UTF-8. Duplicate object keys are errors.
std::expected<Value, ParseError> parse(std::string_view text, Dialect dialect);

} // namespace tk::json
