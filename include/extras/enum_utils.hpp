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

} // namespace extras
