#pragma once

#include <string_view>
#include <type_traits>

namespace extras
{

// Use this to cast `enum class` value to underlying type without much syntax noise:
// ```
// enum class Foo { A, B, C };
// static_assert(to_underlying(Foo::B) == 1);
// ```
template<typename T>
inline constexpr std::underlying_type_t<T> to_underlying(T value) noexcept
{
	return static_cast<std::underlying_type_t<T>>(value);
}

template<typename T>
using enum_size = std::integral_constant<std::underlying_type_t<T>, to_underlying(T::EnumSize)>;

// Use this to get the number of elements in enum if no manual
// value assignment is used and `EnumSize` is the last element:
// ```
// enum class Foo { A, B, C, D, EnumSize };
// static_assert(enum_size_v<Foo> == 4);
// ```
template<typename T>
constexpr inline std::underlying_type_t<T> enum_size_v = enum_size<T>::value;

// This is only a declaration. Modules with enum declarations which need to
// have value->name conversion are expected to provides their own specializations:
// ```
// <foo.hpp>
// namespace foo { enum class Foo { A, B, C }; }
// namespace extras { template<> std::string_view enum_name(foo::Foo value) noexcept; }
// <foo.cpp>
// ...
// namespace extras
// {
// template<> std::string_view enum_name(foo::Foo value) noexcept
// {
//   switch (value) { case A: return "A"; ... };
// }
// }
// ```
template<typename T>
std::string_view enum_name(T value) noexcept;

// Adapter to make enums with bit flag semantics (both scoped and unscoped ones)
// actually behave like bitsets, with bitwise operations and convenient functions.
// Also provides a layer of semantic safety - while the original enum values mean
// exactly single bits, this object explicitly means a combination of bit flags.
//
// To use in your code, simply alias this type:
//    enum class MyFlagBit { A, B, ... };
//    using MyFlags = extras::enum_flags<MyFlagBit>;
//
// You can also explicitly instantiate the template, though this
// probably improves compilation times only with modules, where
// compiler can precompile template instantiations in header units.
template<typename T>
	requires(std::is_enum_v<T>)
struct enum_flags {
	using U = std::underlying_type_t<T>;

	constexpr enum_flags() noexcept = default;
	constexpr explicit enum_flags(T v) noexcept : value(v) {}
	constexpr enum_flags(std::initializer_list<T> il) noexcept
	{
		for (T v : il) {
			value = T(U(value) | U(v));
		}
	}

	// Check that there are no flags set
	constexpr bool empty() const noexcept { return value == T(0); }
	// Check that the given flag is set; zero flags always return true
	constexpr bool test(T v) const noexcept { return (U(value) & U(v)) == U(v); }
	constexpr void set(T v) noexcept { value = T(U(value) | U(v)); }
	constexpr void unset(T v) noexcept { value = T(U(value) & ~U(v)); }
	// Unset all flags
	constexpr void clear() noexcept { value = T(0); }

	// Check that all flags from `rhs` are set; always passes if `rhs` is empty
	constexpr bool test_all(enum_flags rhs) const noexcept { return (U(value) & U(rhs.value)) == U(rhs.value); }
	// Check that at least one flag from `rhs` is set; always fails if `rhs` is empty
	constexpr bool test_any(enum_flags rhs) const noexcept { return (U(value) & U(rhs.value)) != 0; }

	constexpr U to_underlying() const noexcept { return U(value); }
	constexpr static enum_flags from_underlying(U value) noexcept { return enum_flags(T(value)); }

	constexpr bool operator==(const enum_flags &) const noexcept = default;
	constexpr bool operator!=(const enum_flags &) const noexcept = default;

	constexpr enum_flags operator|(T rhs) const noexcept { return from_underlying(U(value) | U(rhs)); }
	constexpr enum_flags operator|(enum_flags rhs) const noexcept { return from_underlying(U(value) | U(rhs.value)); }
	constexpr enum_flags operator&(T rhs) const noexcept { return from_underlying(U(value) & U(rhs)); }
	constexpr enum_flags operator&(enum_flags rhs) const noexcept { return from_underlying(U(value) & U(rhs.value)); }
	constexpr enum_flags operator^(T rhs) const noexcept { return from_underlying(U(value) ^ U(rhs)); }
	constexpr enum_flags operator^(enum_flags rhs) const noexcept { return from_underlying(U(value) ^ U(rhs.value)); }

	constexpr enum_flags operator~() const noexcept { return from_underlying(~U(value)); }

	constexpr enum_flags &operator|=(T rhs) noexcept
	{
		value = T(U(value) | U(rhs));
		return *this;
	}

	constexpr enum_flags &operator|=(enum_flags rhs) noexcept
	{
		value = T(U(value) | U(rhs.value));
		return *this;
	}

	constexpr enum_flags &operator&=(T rhs) noexcept
	{
		value = T(U(value) & U(rhs));
		return *this;
	}

	constexpr enum_flags &operator&=(enum_flags rhs) noexcept
	{
		value = T(U(value) & U(rhs.value));
		return *this;
	}

	constexpr enum_flags &operator^=(T rhs) noexcept
	{
		value = T(U(value) ^ U(rhs));
		return *this;
	}

	constexpr enum_flags &operator^=(enum_flags rhs) noexcept
	{
		value = T(U(value) ^ U(rhs.value));
		return *this;
	}

	// Stored as enum, not the underlying type so that debuggers
	// can pretty-print it as flags without any further setup.
	// Also publicly available if you need more manual usage.
	T value = T(0);
};

} // namespace extras
