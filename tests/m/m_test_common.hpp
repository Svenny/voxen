#pragma once

#include <m/m.hpp>

#include <cstddef>

// Helpers shared by the `m` tests

namespace m
{

// Largest absolute difference over components
template<CScalar T, size_t N>
T maxDifference(const vector<T, N> &a, const vector<T, N> &b) noexcept
{
	T result = T(0);
	for (size_t i = 0; i < N; i++) {
		result = max(result, abs(a[i] - b[i]));
	}
	return result;
}

template<CScalar T, size_t R, size_t C>
T maxDifference(const matrix<T, R, C> &a, const matrix<T, R, C> &b) noexcept
{
	T result = T(0);
	for (size_t i = 0; i < R; i++) {
		result = max(result, maxDifference(a[i], b[i]));
	}
	return result;
}

// Compares rotations, `q` and `-q` are the same one
template<CFloating T>
bool sameRotation(const quaternion<T> &a, const quaternion<T> &b, T eps) noexcept
{
	return abs(dot(a, b)) > T(1) - eps;
}

} // namespace m
