#pragma once

#include <m/format.hpp>

// IWYU pragma: begin_exports
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_tostring.hpp>
#include <catch2/matchers/catch_matchers.hpp>
// IWYU pragma: end_exports

#include <cstddef>
#include <string>
#include <system_error>

namespace Catch
{

template<>
struct StringMaker<std::error_code> {
	static std::string convert(const std::error_code &ec);
};

template<>
struct StringMaker<std::error_condition> {
	static std::string convert(const std::error_condition &ec);
};

template<typename T, size_t N>
struct StringMaker<m::vector<T, N>> {
	static std::string convert(const m::vector<T, N> &value) { return fmt::format("{}", value); }
};

template<typename T, size_t R, size_t C>
struct StringMaker<m::matrix<T, R, C>> {
	static std::string convert(const m::matrix<T, R, C> &value) { return fmt::format("{}", value); }
};

template<typename T>
struct StringMaker<m::quaternion<T>> {
	static std::string convert(const m::quaternion<T> &value) { return fmt::format("{}", value); }
};

} // namespace Catch
