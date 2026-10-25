#pragma once

#include <m/vector.hpp>

#include <cassert>
#include <cstddef>

// Row-major matrices used with column vectors.
//
// `matrix<T, R, C>` has `R` rows of `C` components. `a[i]` is row `i` and `a[i][j]`
// is the element at row `i`, column `j`, the same as in HLSL/Slang. Rows are
// contiguous in memory - the layout Slang produces with `-matrix-layout-row-major`
// (and GLSL with `layout(row_major)`), so matrices can be copied to GPU as-is.
//
// Vectors are columns: a transform is applied as `mul(a, v)`, and `mul(a, b)` applies
// `b` first, then `a`. Translation lives in the last column, which makes `float3x4`
// a complete affine transform with the last row `0 0 0 1` implied.
//
// There is no `operator*`. HLSL defines it componentwise while GLM (and most C++
// code) as the product, `mul()` turns this trap into a compile error in both
// directions. There is no row-vector `mul(v, a)` either: swapped arguments are
// an error, not a silently transposed transform.
//
// Matrices are zero-initialized by default, use `identity()` for the identity.

namespace m
{

template<typename T, size_t R, size_t C>
struct matrix {
	static_assert(CScalar<T>);
	static_assert(R >= 2 && R <= 4 && C >= 2 && C <= 4);

	using Row = vector<T, C>;
	using Column = vector<T, R>;

	Row row[R] = {};

	constexpr matrix() noexcept = default;

	constexpr matrix(const Row &r0, const Row &r1) noexcept
		requires(R == 2)
		: row { r0, r1 }
	{}

	constexpr matrix(const Row &r0, const Row &r1, const Row &r2) noexcept
		requires(R == 3)
		: row { r0, r1, r2 }
	{}

	constexpr matrix(const Row &r0, const Row &r1, const Row &r2, const Row &r3) noexcept
		requires(R == 4)
		: row { r0, r1, r2, r3 }
	{}

	template<CScalar U>
	constexpr explicit matrix(const matrix<U, R, C> &a) noexcept
	{
		for (size_t i = 0; i < R; i++) {
			row[i] = Row(a.row[i]);
		}
	}

	static constexpr matrix identity() noexcept
		requires(R == C)
	{
		matrix a;
		for (size_t i = 0; i < R; i++) {
			a.row[i][i] = T(1);
		}
		return a;
	}

	constexpr Row &operator[](size_t i) noexcept
	{
		assert(i < R);
		return row[i];
	}

	constexpr const Row &operator[](size_t i) const noexcept
	{
		assert(i < R);
		return row[i];
	}

	constexpr bool operator==(const matrix &) const noexcept = default;
};

template<CScalar T, size_t R, size_t C>
constexpr vector<T, R> column(const matrix<T, R, C> &a, size_t j) noexcept
{
	return detail::make<R>([&](size_t i) { return a[i][j]; });
}

template<CScalar T, size_t R, size_t C>
constexpr matrix<T, C, R> transpose(const matrix<T, R, C> &a) noexcept
{
	matrix<T, C, R> result;
	for (size_t i = 0; i < R; i++) {
		for (size_t j = 0; j < C; j++) {
			result[j][i] = a[i][j];
		}
	}
	return result;
}

// Matrix product, `b` is applied first when both are transforms
template<CScalar T, size_t R, size_t K, size_t C>
constexpr matrix<T, R, C> mul(const matrix<T, R, K> &a, const matrix<T, K, C> &b) noexcept
{
	matrix<T, R, C> result;
	for (size_t i = 0; i < R; i++) {
		result[i] = detail::make<C>([&](size_t j) { return dot(a[i], column(b, j)); });
	}
	return result;
}

// Product of affine transforms, each is a 4x4 matrix with the `0 0 0 1` last row implied
template<CScalar T>
constexpr matrix<T, 3, 4> mul(const matrix<T, 3, 4> &a, const matrix<T, 3, 4> &b) noexcept
{
	matrix<T, 3, 4> result;
	for (size_t i = 0; i < 3; i++) {
		result[i] = detail::make<4>([&](size_t j) {
			const T sum = dot(a[i].xyz(), detail::make<3>([&](size_t k) { return b[k][j]; }));
			return j == 3 ? T(sum + a[i].w) : sum;
		});
	}
	return result;
}

// Transforms a column vector
template<CScalar T, size_t R, size_t C>
constexpr vector<T, R> mul(const matrix<T, R, C> &a, const vector<T, C> &v) noexcept
{
	return detail::make<R>([&](size_t i) { return dot(a[i], v); });
}

template<CScalar T>
constexpr T determinant(const matrix<T, 2, 2> &a) noexcept
{
	return T(a[0][0] * a[1][1] - a[0][1] * a[1][0]);
}

template<CScalar T>
constexpr T determinant(const matrix<T, 3, 3> &a) noexcept
{
	return T(a[0][0] * T(a[1][1] * a[2][2] - a[1][2] * a[2][1]) - a[0][1] * T(a[1][0] * a[2][2] - a[1][2] * a[2][0])
		+ a[0][2] * T(a[1][0] * a[2][1] - a[1][1] * a[2][0]));
}

template<CScalar T>
constexpr T determinant(const matrix<T, 4, 4> &a) noexcept
{
	// Laplace expansion with 2x2 minors of the top and bottom halves
	const T s0 = T(a[0][0] * a[1][1] - a[1][0] * a[0][1]);
	const T s1 = T(a[0][0] * a[1][2] - a[1][0] * a[0][2]);
	const T s2 = T(a[0][0] * a[1][3] - a[1][0] * a[0][3]);
	const T s3 = T(a[0][1] * a[1][2] - a[1][1] * a[0][2]);
	const T s4 = T(a[0][1] * a[1][3] - a[1][1] * a[0][3]);
	const T s5 = T(a[0][2] * a[1][3] - a[1][2] * a[0][3]);

	const T c5 = T(a[2][2] * a[3][3] - a[3][2] * a[2][3]);
	const T c4 = T(a[2][1] * a[3][3] - a[3][1] * a[2][3]);
	const T c3 = T(a[2][1] * a[3][2] - a[3][1] * a[2][2]);
	const T c2 = T(a[2][0] * a[3][3] - a[3][0] * a[2][3]);
	const T c1 = T(a[2][0] * a[3][2] - a[3][0] * a[2][2]);
	const T c0 = T(a[2][0] * a[3][1] - a[3][0] * a[2][1]);

	return T(s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0);
}

// Inverses divide by the determinant, singular matrices give inf/nan

template<CFloating T>
constexpr matrix<T, 2, 2> inverse(const matrix<T, 2, 2> &a) noexcept
{
	const T inv = T(1) / determinant(a);
	return {
		{ a[1][1] * inv, -a[0][1] * inv },
		{ -a[1][0] * inv, a[0][0] * inv },
	};
}

template<CFloating T>
constexpr matrix<T, 3, 3> inverse(const matrix<T, 3, 3> &a) noexcept
{
	const T inv = T(1) / determinant(a);
	return {
		{
			(a[1][1] * a[2][2] - a[1][2] * a[2][1]) * inv,
			(a[0][2] * a[2][1] - a[0][1] * a[2][2]) * inv,
			(a[0][1] * a[1][2] - a[0][2] * a[1][1]) * inv,
		},
		{
			(a[1][2] * a[2][0] - a[1][0] * a[2][2]) * inv,
			(a[0][0] * a[2][2] - a[0][2] * a[2][0]) * inv,
			(a[0][2] * a[1][0] - a[0][0] * a[1][2]) * inv,
		},
		{
			(a[1][0] * a[2][1] - a[1][1] * a[2][0]) * inv,
			(a[0][1] * a[2][0] - a[0][0] * a[2][1]) * inv,
			(a[0][0] * a[1][1] - a[0][1] * a[1][0]) * inv,
		},
	};
}

template<CFloating T>
constexpr matrix<T, 4, 4> inverse(const matrix<T, 4, 4> &a) noexcept
{
	const T s0 = a[0][0] * a[1][1] - a[1][0] * a[0][1];
	const T s1 = a[0][0] * a[1][2] - a[1][0] * a[0][2];
	const T s2 = a[0][0] * a[1][3] - a[1][0] * a[0][3];
	const T s3 = a[0][1] * a[1][2] - a[1][1] * a[0][2];
	const T s4 = a[0][1] * a[1][3] - a[1][1] * a[0][3];
	const T s5 = a[0][2] * a[1][3] - a[1][2] * a[0][3];

	const T c5 = a[2][2] * a[3][3] - a[3][2] * a[2][3];
	const T c4 = a[2][1] * a[3][3] - a[3][1] * a[2][3];
	const T c3 = a[2][1] * a[3][2] - a[3][1] * a[2][2];
	const T c2 = a[2][0] * a[3][3] - a[3][0] * a[2][3];
	const T c1 = a[2][0] * a[3][2] - a[3][0] * a[2][2];
	const T c0 = a[2][0] * a[3][1] - a[3][0] * a[2][1];

	const T inv = T(1) / (s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0);

	return {
		{
			(a[1][1] * c5 - a[1][2] * c4 + a[1][3] * c3) * inv,
			(-a[0][1] * c5 + a[0][2] * c4 - a[0][3] * c3) * inv,
			(a[3][1] * s5 - a[3][2] * s4 + a[3][3] * s3) * inv,
			(-a[2][1] * s5 + a[2][2] * s4 - a[2][3] * s3) * inv,
		},
		{
			(-a[1][0] * c5 + a[1][2] * c2 - a[1][3] * c1) * inv,
			(a[0][0] * c5 - a[0][2] * c2 + a[0][3] * c1) * inv,
			(-a[3][0] * s5 + a[3][2] * s2 - a[3][3] * s1) * inv,
			(a[2][0] * s5 - a[2][2] * s2 + a[2][3] * s1) * inv,
		},
		{
			(a[1][0] * c4 - a[1][1] * c2 + a[1][3] * c0) * inv,
			(-a[0][0] * c4 + a[0][1] * c2 - a[0][3] * c0) * inv,
			(a[3][0] * s4 - a[3][1] * s2 + a[3][3] * s0) * inv,
			(-a[2][0] * s4 + a[2][1] * s2 - a[2][3] * s0) * inv,
		},
		{
			(-a[1][0] * c3 + a[1][1] * c1 - a[1][2] * c0) * inv,
			(a[0][0] * c3 - a[0][1] * c1 + a[0][2] * c0) * inv,
			(-a[3][0] * s3 + a[3][1] * s1 - a[3][2] * s0) * inv,
			(a[2][0] * s3 - a[2][1] * s1 + a[2][2] * s0) * inv,
		},
	};
}

// Inverse of an affine transform (4x4 with the `0 0 0 1` last row implied)
template<CFloating T>
constexpr matrix<T, 3, 4> inverseAffine(const matrix<T, 3, 4> &a) noexcept
{
	const matrix<T, 3, 3> linear = inverse(matrix<T, 3, 3>(a[0].xyz(), a[1].xyz(), a[2].xyz()));
	const vector<T, 3> translation = column(a, 3);

	matrix<T, 3, 4> result;
	for (size_t i = 0; i < 3; i++) {
		result[i] = { linear[i], -dot(linear[i], translation) };
	}
	return result;
}

} // namespace m
