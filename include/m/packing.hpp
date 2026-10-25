#pragma once

#include <m/vector.hpp>

#include <concepts>
#include <cstddef>
#include <limits>
#include <type_traits>

// Conversions between floating-point values and normalized integers, as in GPU
// vertex and texture formats: unorm maps [0, 1] to [0, max] and snorm maps [-1, 1]
// to [-max, max] (the most negative integer is never produced, unpacking clamps it).
// Packing clamps to the range and rounds to nearest. Vectors are processed componentwise.
// Packing NaN is undefined behaviour: it survives the clamp and then converts to an integer.

namespace m
{

template<typename U>
concept CUnormInteger = std::unsigned_integral<U> && !std::same_as<U, bool> && sizeof(U) <= 4;

template<typename I>
concept CSnormInteger = std::signed_integral<I> && sizeof(I) <= 4;

namespace detail
{

// Type to scale in, 32-bit integer range is not representable in `float`
template<typename I, CFloating F>
using PackingIntermediate = std::conditional_t<(sizeof(I) > 2), double, F>;

} // namespace detail

template<CUnormInteger U, CFloating F>
constexpr U packUnorm(F x) noexcept
{
	using W = detail::PackingIntermediate<U, F>;
	return U(round(W(saturate(x)) * W(std::numeric_limits<U>::max())));
}

template<CUnormInteger U, CFloating F, size_t N>
constexpr vector<U, N> packUnorm(const vector<F, N> &v) noexcept
{
	return detail::make<N>([&](size_t i) { return packUnorm<U>(v[i]); });
}

template<CFloating F, CUnormInteger U>
constexpr F unpackUnorm(U x) noexcept
{
	return F(x) / F(std::numeric_limits<U>::max());
}

template<CFloating F, CUnormInteger U, size_t N>
constexpr vector<F, N> unpackUnorm(const vector<U, N> &v) noexcept
{
	return detail::make<N>([&](size_t i) { return unpackUnorm<F>(v[i]); });
}

template<CSnormInteger I, CFloating F>
constexpr I packSnorm(F x) noexcept
{
	using W = detail::PackingIntermediate<I, F>;
	return I(round(W(clamp(x, F(-1), F(1))) * W(std::numeric_limits<I>::max())));
}

template<CSnormInteger I, CFloating F, size_t N>
constexpr vector<I, N> packSnorm(const vector<F, N> &v) noexcept
{
	return detail::make<N>([&](size_t i) { return packSnorm<I>(v[i]); });
}

template<CFloating F, CSnormInteger I>
constexpr F unpackSnorm(I x) noexcept
{
	return max(F(x) / F(std::numeric_limits<I>::max()), F(-1));
}

template<CFloating F, CSnormInteger I, size_t N>
constexpr vector<F, N> unpackSnorm(const vector<I, N> &v) noexcept
{
	return detail::make<N>([&](size_t i) { return unpackSnorm<F>(v[i]); });
}

} // namespace m
