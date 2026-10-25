#include <m/format.hpp>
#include <m/m.hpp>

#include "../test_common.hpp"
#include "m_test_common.hpp"

#include <fmt/format.h>

#include <cstdint>
#include <cstring>
#include <type_traits>

namespace m
{

namespace
{

static_assert(sizeof(float4x4) == 64 && sizeof(float3x4) == 48 && alignof(float3x4) == 4);
static_assert(sizeof(double3x3) == 72 && alignof(double3x3) == 8);
static_assert(std::is_trivially_copyable_v<float4x4> && std::is_standard_layout_v<float4x4>);
static_assert(!std::is_convertible_v<double4x4, float4x4> && std::is_constructible_v<float4x4, double4x4>);
// No `operator*`, products are spelled `mul()`, and only with a column vector
template<typename A, typename B = A>
concept CMultipliable = requires(A a, B b) { a * b; };
template<typename A, typename B>
concept CMullable = requires(A a, B b) { mul(a, b); };
static_assert(!CMultipliable<float4x4> && !CMultipliable<float4x4, float4> && !CMultipliable<float4, float4x4>);
static_assert(CMullable<float4x4, float4> && !CMullable<float4, float4x4>);

// `a[i]` is row `i`, `a[i][j]` is at row `i` column `j`
constexpr float3x4 A { { 1, 2, 3, 4 }, { 5, 6, 7, 8 }, { 9, 10, 11, 12 } };
static_assert(A[1] == float4(5, 6, 7, 8) && A[1][2] == 7);
static_assert(column(A, 3) == float3(4, 8, 12));
static_assert(float2x2() == float2x2({ 0, 0 }, { 0, 0 }));
static_assert(float3x3::identity() == float3x3({ 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }));
static_assert(transpose(A) == float4x3({ 1, 5, 9 }, { 2, 6, 10 }, { 3, 7, 11 }, { 4, 8, 12 }));
static_assert(float3x4(double3x4(A)) == A);

// Products
static_assert(mul(float3x3::identity(), A) == A);
static_assert(mul(A, float4x4::identity()) == A);
static_assert(mul(A, float4(1, 1, 1, 1)) == float3(10, 26, 42));
static_assert(
	mul(float2x3({ 1, 2, 3 }, { 4, 5, 6 }), float3x2({ 1, 0 }, { 0, 1 }, { 1, 1 })) == float2x2({ 4, 5 }, { 10, 11 }));

// Column vectors: `mul(a, b)` applies `b` first. Translation lives in the last column.
constexpr float3x4 T { { 1, 0, 0, 1 }, { 0, 1, 0, 2 }, { 0, 0, 1, 3 } };
constexpr float3x4 S { { 2, 0, 0, 0 }, { 0, 2, 0, 0 }, { 0, 0, 2, 0 } };
static_assert(mul(T, float4(0, 0, 0, 1)) == float3(1, 2, 3));
static_assert(mul(mul(S, T), float4(0, 0, 0, 1)) == float3(2, 4, 6));
static_assert(mul(mul(T, S), float4(0, 0, 0, 1)) == float3(1, 2, 3));

// Affine product agrees with the 4x4 one
static_assert(homogeneous(mul(S, T)) == mul(homogeneous(S), homogeneous(T)));
static_assert(homogeneous(mul(T, S)) == mul(homogeneous(T), homogeneous(S)));

// Determinants
static_assert(determinant(matrix<int32_t, 2, 2>({ 1, 2 }, { 3, 4 })) == -2);
static_assert(determinant(float3x3({ 2, 0, 0 }, { 0, 3, 0 }, { 0, 0, 4 })) == 24);
static_assert(determinant(float3x3({ 0, 1, 0 }, { 1, 0, 0 }, { 0, 0, 1 })) == -1);
static_assert(determinant(float4x4({ 1, 2, 3, 4 }, { 0, 1, 2, 3 }, { 0, 0, 1, 2 }, { 0, 0, 0, 1 })) == 1);
static_assert(determinant(float4x4({ 0, 1, 0, 0 }, { 1, 0, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 })) == -1);
static_assert(determinant(homogeneous(S)) == 8);
static_assert(determinant(float4x4({ 1, 2, 3, 4 }, { 2, 4, 6, 8 }, { 0, 1, 0, 1 }, { 5, 0, 1, 0 })) == 0);

// Exact inverses
static_assert(inverse(float2x2({ 2, 0 }, { 0, 4 })) == float2x2({ 0.5f, 0 }, { 0, 0.25f }));
static_assert(inverse(homogeneous(T)) == homogeneous(float3x4({ 1, 0, 0, -1 }, { 0, 1, 0, -2 }, { 0, 0, 1, -3 })));
static_assert(inverseAffine(T) == float3x4({ 1, 0, 0, -1 }, { 0, 1, 0, -2 }, { 0, 0, 1, -3 }));

} // namespace

TEST_CASE("Rows are contiguous in memory", "[m::matrix]")
{
	float raw[12];
	std::memcpy(raw, &A, sizeof(A));
	size_t mismatches = 0;
	for (size_t i = 0; i < 3; i++) {
		for (size_t j = 0; j < 4; j++) {
			mismatches += raw[i * 4 + j] != A[i][j];
		}
	}
	CHECK(mismatches == 0);
}

TEST_CASE("Inverses undo their matrices", "[m::matrix]")
{
	const double2x2 a2 { { 4, 7 }, { 2, 6 } };
	CHECK(maxDifference(mul(inverse(a2), a2), double2x2::identity()) < 1e-12);

	const double3x3 a3 { { 2, -1, 0 }, { 1, 3, 2 }, { -3, 0.5, 4 } };
	CHECK(maxDifference(mul(a3, inverse(a3)), double3x3::identity()) < 1e-12);

	const double4x4 a4 { { 2, 1, 0, 3 }, { 1, 3, 2, 0 }, { 0, 1, 4, 1 }, { 1, 0, 2, 5 } };
	CHECK(maxDifference(mul(inverse(a4), a4), double4x4::identity()) < 1e-12);
	CHECK(maxDifference(mul(a4, inverse(a4)), double4x4::identity()) < 1e-12);

	// Rotation about Z by ~30 degrees, scale 2 and a translation
	const double3x4 affine { { 1.7320508, -1, 0, 5 }, { 1, 1.7320508, 0, -3 }, { 0, 0, 2, 10 } };
	const double3x4 inv = inverseAffine(affine);
	CHECK(maxDifference(mul(inv, affine), double3x4({ 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 })) < 1e-12);

	// Same as the 4x4 inverse of the homogeneous matrix
	const double4x4 h = homogeneous(affine);
	const double4x4 hinv = inverse(h);
	CHECK(maxDifference(double3x4(hinv[0], hinv[1], hinv[2]), inv) < 1e-12);
}

TEST_CASE("Matrix formatting", "[m::matrix]")
{
	CHECK(fmt::format("{}", float2x2({ 1, 2 }, { 3, 4 })) == "[(1, 2), (3, 4)]");
	CHECK(fmt::format("{:.1f}", double2x3({ 1, 2, 3 }, { 4, 5, 6 })) == "[(1.0, 2.0, 3.0), (4.0, 5.0, 6.0)]");
}

} // namespace m
