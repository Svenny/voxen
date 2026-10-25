#include <m/format.hpp>
#include <m/m.hpp>

#include "../test_common.hpp"
#include "m_test_common.hpp"

#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <fmt/format.h>

#include <cstddef>
#include <numbers>
#include <type_traits>

namespace m
{

namespace
{

constexpr double PI = std::numbers::pi;

static_assert(sizeof(fquat) == 16 && sizeof(dquat) == 32);
static_assert(std::is_trivially_copyable_v<fquat> && std::is_standard_layout_v<fquat>);
static_assert(!std::is_convertible_v<float4, fquat> && std::is_constructible_v<fquat, float4>);
static_assert(!std::is_constructible_v<fquat, float, float, float, float>);
template<typename A, typename B = A>
concept CMultipliable = requires(A a, B b) { a * b; };
static_assert(!CMultipliable<fquat> && !CMultipliable<fquat, float3>);

static_assert(fquat() == fquat::identity());
static_assert(fquat().toVector() == float4(0, 0, 0, 1));
static_assert(mul(fquat(), float3(1, 2, 3)) == float3(1, 2, 3));
static_assert(mul(fquat::identity(), fquat::identity()) == fquat::identity());
static_assert(conjugate(fquat(float4(1, 2, 3, 4))) == fquat(float4(-1, -2, -3, 4)));
static_assert(dot(fquat(float4(1, 2, 3, 4)), fquat(float4(1, 1, 1, 1))) == 10);
static_assert(toMatrix(dquat()) == double3x3::identity());
static_assert(dquat::fromRotationMatrix(double3x3::identity()) == dquat());
// 180 degrees about Z: (x, y, z) -> (-x, -y, z)
static_assert(mul(dquat(double4(0, 0, 1, 0)), double3(1, 2, 3)) == double3(-1, -2, 3));
static_assert(toMatrix(dquat(double4(0, 0, 1, 0))) == double3x3({ -1, 0, 0 }, { 0, -1, 0 }, { 0, 0, 1 }));

} // namespace

TEST_CASE("Axis-angle rotations follow the right-hand rule", "[m::quaternion]")
{
	const dquat about_x = dquat::fromAxisAngle(double3(1, 0, 0), PI / 2);
	const dquat about_y = dquat::fromAxisAngle(double3(0, 1, 0), PI / 2);
	const dquat about_z = dquat::fromAxisAngle(double3(0, 0, 1), PI / 2);

	CHECK(maxDifference(mul(about_x, double3(0, 1, 0)), double3(0, 0, 1)) < 1e-12);
	CHECK(maxDifference(mul(about_y, double3(0, 0, 1)), double3(1, 0, 0)) < 1e-12);
	CHECK(maxDifference(mul(about_z, double3(1, 0, 0)), double3(0, 1, 0)) < 1e-12);
	// Clockwise the other way
	CHECK(maxDifference(mul(about_y, double3(1, 0, 0)), double3(0, 0, -1)) < 1e-12);

	// Angle is modulo 2pi and the quaternion stays unit
	CHECK_THAT(length(about_x), Catch::Matchers::WithinAbs(1.0, 1e-12));
	CHECK(sameRotation(dquat::fromAxisAngle(double3(1, 0, 0), PI / 2 + 2 * PI), about_x, 1e-12));
}

TEST_CASE("Quaternion products compose like matrices", "[m::quaternion]")
{
	const dquat a = dquat::fromAxisAngle(normalize(double3(1, 2, 3)), 0.7);
	const dquat b = dquat::fromAxisAngle(normalize(double3(-2, 1, 0.5)), -2.1);
	const double3 v(0.3, -4, 2.5);

	// `b` is applied first
	CHECK(maxDifference(mul(mul(a, b), v), mul(a, mul(b, v))) < 1e-12);
	CHECK(maxDifference(mul(toMatrix(mul(a, b)), v), mul(toMatrix(a), mul(toMatrix(b), v))) < 1e-12);
	// Matrix and quaternion rotations agree
	CHECK(maxDifference(mul(toMatrix(a), v), mul(a, v)) < 1e-12);
	CHECK_THAT(determinant(toMatrix(a)), Catch::Matchers::WithinAbs(1.0, 1e-12));
	// Two quarter turns about Z make a half turn
	const dquat quarter = dquat::fromAxisAngle(double3(0, 0, 1), PI / 2);
	CHECK(sameRotation(mul(quarter, quarter), dquat(double4(0, 0, 1, 0)), 1e-12));
}

TEST_CASE("Conjugate and inverse undo rotations", "[m::quaternion]")
{
	const dquat q = dquat::fromAxisAngle(normalize(double3(1, -1, 1)), 1.3);
	const double3 v(1, 2, 3);
	CHECK(maxDifference(mul(conjugate(q), mul(q, v)), v) < 1e-12);
	CHECK(sameRotation(mul(inverse(q), q), dquat::identity(), 1e-12));

	// `inverse` works for non-unit quaternions too
	const dquat scaled(q.toVector() * 2.0);
	const dquat product = mul(inverse(scaled), scaled);
	CHECK(maxDifference(product.toVector().xyz(), double3(0)) < 1e-12);
	CHECK_THAT(product.w, Catch::Matchers::WithinAbs(1.0, 1e-12));
	CHECK_THAT(length(normalize(scaled)), Catch::Matchers::WithinAbs(1.0, 1e-12));
}

TEST_CASE("Rotation matrix conversion round-trips", "[m::quaternion]")
{
	// Covers all branches of the conversion: large w, and large x, y or z
	const dquat cases[] = {
		dquat::fromAxisAngle(normalize(double3(1, 2, 3)), 0.3),
		dquat::fromAxisAngle(double3(1, 0, 0), PI - 0.01),
		dquat::fromAxisAngle(double3(0, 1, 0), PI - 0.01),
		dquat::fromAxisAngle(double3(0, 0, 1), PI - 0.01),
		dquat::fromAxisAngle(normalize(double3(1, 1, 0)), PI),
		dquat::fromAxisAngle(normalize(double3(0, -1, 1)), 2.5),
	};

	size_t mismatches = 0;
	for (const dquat &q : cases) {
		if (!sameRotation(dquat::fromRotationMatrix(toMatrix(q)), q, 1e-12)) {
			mismatches++;
			UNSCOPED_INFO(fmt::format("{} -> {}", q, dquat::fromRotationMatrix(toMatrix(q))));
		}
	}
	CHECK(mismatches == 0);
}

TEST_CASE("Spherical interpolation", "[m::quaternion]")
{
	const dquat a = dquat::fromAxisAngle(double3(0, 0, 1), 0.2);
	const dquat b = dquat::fromAxisAngle(double3(0, 0, 1), 0.2 + PI / 2);
	const dquat mid = dquat::fromAxisAngle(double3(0, 0, 1), 0.2 + PI / 4);

	CHECK(sameRotation(slerp(a, b, 0.0), a, 1e-12));
	CHECK(sameRotation(slerp(a, b, 1.0), b, 1e-12));
	CHECK(sameRotation(slerp(a, b, 0.5), mid, 1e-12));
	// The shorter arc is taken even if the other quaternion is negated
	CHECK(sameRotation(slerp(a, dquat(-b.toVector()), 0.5), mid, 1e-12));
	// Nearly equal inputs don't blow up
	const dquat close = dquat::fromAxisAngle(double3(0, 0, 1), 0.2 + 1e-9);
	CHECK(sameRotation(slerp(a, close, 0.5), a, 1e-12));
	CHECK_THAT(length(slerp(a, close, 0.5)), Catch::Matchers::WithinAbs(1.0, 1e-12));
}

TEST_CASE("Quaternion formatting", "[m::quaternion]")
{
	CHECK(fmt::format("{}", fquat()) == "(0, 0, 0, 1)");
	CHECK(fmt::format("{:.1f}", dquat(double4(0.5, 0, 0, 0.25))) == "(0.5, 0.0, 0.0, 0.2)");
}

} // namespace m
