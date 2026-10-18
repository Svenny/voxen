#include <m/m.hpp>

#include "../test_common.hpp"
#include "m_test_common.hpp"

#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <numbers>

namespace m
{

namespace
{

constexpr double PI = std::numbers::pi;
constexpr double3x4 IDENTITY_3X4 { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } };

static_assert(mul(translation(float3(1, 2, 3)), float4(10, 20, 30, 1)) == float3(11, 22, 33));
static_assert(mul(translation(float3(1, 2, 3)), float4(10, 20, 30, 0)) == float3(10, 20, 30));
static_assert(mul(scaling(float3(2, 3, 4)), float4(1, 1, 1, 1)) == float3(2, 3, 4));
static_assert(rotation(fquat()) == float3x4({ 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }));
static_assert(rigidTransform(dquat(), double3(1, 2, 3)) == translation(double3(1, 2, 3)));
static_assert(viewFromCamera(dquat(), double3(1, 2, 3)) == translation(double3(-1, -2, -3)));
static_assert(homogeneous(translation(float3(1, 2, 3)))
	== float4x4({ 1, 0, 0, 1 }, { 0, 1, 0, 2 }, { 0, 0, 1, 3 }, { 0, 0, 0, 1 }));
// Transforms compose right to left: scale first, then translate
static_assert(mul(mul(translation(float3(1, 0, 0)), scaling(float3(2, 2, 2))), float4(1, 1, 1, 1)) == float3(3, 2, 2));

} // namespace

TEST_CASE("View matrix inverts the camera placement", "[m::transform]")
{
	const dquat orientation = dquat::fromAxisAngle(normalize(double3(1, 2, -1)), 1.1);
	const double3 position(10, -20, 30);

	const double3x4 camera_to_world = rigidTransform(orientation, position);
	const double3x4 world_to_camera = viewFromCamera(orientation, position);
	CHECK(maxDifference(mul(world_to_camera, camera_to_world), IDENTITY_3X4) < 1e-12);
	CHECK(maxDifference(world_to_camera, inverseAffine(camera_to_world)) < 1e-12);
	CHECK(maxDifference(mul(rotation(orientation), double4(1, 2, 3, 1)), mul(orientation, double3(1, 2, 3))) < 1e-12);
}

TEST_CASE("Camera looks along -Z of its orientation", "[m::transform]")
{
	// Identity orientation: a point 5 m in front of the camera is at view-space -Z
	const double3 position(100, 200, 300);
	CHECK(
		maxDifference(mul(viewFromCamera(dquat(), position), double4(position + double3(0, 0, -5), 1)), double3(0, 0, -5))
		< 1e-12);

	// Turned 90 degrees to the left (about +Y), the camera looks along -X
	const dquat turned_left = dquat::fromAxisAngle(double3(0, 1, 0), PI / 2);
	CHECK(maxDifference(mul(turned_left, double3(0, 0, -1)), double3(-1, 0, 0)) < 1e-12);
	CHECK(maxDifference(mul(viewFromCamera(turned_left, position), double4(position + double3(-5, 0, 0), 1)),
				double3(0, 0, -5))
		< 1e-12);
	// And what was in front is now on the right
	CHECK(maxDifference(mul(viewFromCamera(turned_left, position), double4(position + double3(0, 0, -5), 1)),
				double3(5, 0, 0))
		< 1e-12);
}

TEST_CASE("Infinite reversed-Z projection", "[m::transform]")
{
	using Catch::Matchers::WithinAbs;

	const double z_near = 0.5;
	const double4x4 proj = perspectiveInfinite(PI / 2, 2.0, z_near);

	auto to_ndc = [&](const double3 &view_pos) {
		const double4 clip = mul(proj, double4(view_pos, 1));
		return clip.xyz() / clip.w;
	};

	// Near plane is at depth 1, infinity at 0, depth decreases monotonically
	CHECK_THAT(to_ndc(double3(0, 0, -z_near)).z, WithinAbs(1.0, 1e-12));
	CHECK_THAT(to_ndc(double3(0, 0, -1e9)).z, WithinAbs(0.0, 1e-8));
	CHECK(to_ndc(double3(0, 0, -1)).z > to_ndc(double3(0, 0, -2)).z);
	// Points behind the near plane have negative clip W
	CHECK(mul(proj, double4(0, 0, 1, 1)).w < 0);

	// With a 90 degree vertical FOV, the top edge at distance 1 is Y = 1 in view space,
	// which is NDC Y = -1 (Vulkan has Y down). Aspect 2 puts the right edge at X = 2.
	CHECK_THAT(to_ndc(double3(0, 1, -1)).y, WithinAbs(-1.0, 1e-12));
	CHECK_THAT(to_ndc(double3(0, -1, -1)).y, WithinAbs(1.0, 1e-12));
	CHECK_THAT(to_ndc(double3(2, 0, -1)).x, WithinAbs(1.0, 1e-12));
	CHECK_THAT(to_ndc(double3(-1, 0, -1)).x, WithinAbs(-0.5, 1e-12));
}

} // namespace m
