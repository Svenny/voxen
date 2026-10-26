#include <m/color.hpp>

#include "../test_common.hpp"

#include <m/vector.hpp>

#include <bit>
#include <cstdint>

namespace m
{

namespace
{

static_assert(sizeof(rgba8_unorm) == 4 && sizeof(rgba8_srgb) == 4);
static_assert(rgba8_unorm(float4(0.0f, 0.5f, 1.0f, 2.0f)) == rgba8_unorm(0, 128, 255, 255));
static_assert(rgba8_unorm(0x12ABCDEFu).toUint32() == 0x12ABCDEFu);

} // namespace

TEST_CASE("'rgba8' sanity check", "[m::color]")
{
	const float3 C1 = float3(20, 40, 50) / 255.0f;
	const float4 C2 = float4(4, 47, 240, 192) / 255.0f;
	const float3 C3 = float3(0.9f, 127.5f, 255.0f) / 255.0f;

	rgba8_srgb srgb1(C1);
	rgba8_srgb srgb2(C2);
	rgba8_srgb srgb3(C3);

	// sRGB conversion and quantization are lossy, check representations manually
	CHECK(srgb1 == rgba8_srgb { 79, 110, 122 });
	CHECK(srgb2 == rgba8_srgb { 34, 119, 248, 192 });
	CHECK(srgb3 == rgba8_srgb { 12, 188, 255 });

	// Round-trip check, must pass if the rounding is done properly
	CHECK(rgba8_srgb(srgb1.toFloat3()) == srgb1);
	CHECK(rgba8_srgb(srgb2.toFloat4()) == srgb2);
	CHECK(rgba8_srgb(srgb3.toFloat3()) == srgb3);

	rgba8_unorm rgb1(C1);
	rgba8_unorm rgb2(C2);
	rgba8_unorm rgb3(C3);

	CHECK(rgb1.toFloat3() == C1);
	CHECK(rgb2.toFloat4() == C2);
	// `rgb3` won't be equal to the original color
	// due to quantization, just check round-trip
	CHECK(rgba8_unorm(rgb3.toFloat3()) == rgb3);

	// Re-encoding round-trip
	CHECK(rgba8_srgb(rgba8_unorm(srgb2)) == srgb2);
	CHECK(rgba8_unorm(srgb1).a == srgb1.a);

	// These trivial colors must have the same representation
	CHECK(rgba8_srgb::opaqueBlack().toUint32() == rgba8_unorm::opaqueBlack().toUint32());
	CHECK(rgba8_srgb::opaqueWhite().toUint32() == rgba8_unorm::opaqueWhite().toUint32());
	CHECK(rgba8_srgb::transparentBlack().toUint32() == rgba8_unorm::transparentBlack().toUint32());
	CHECK(rgba8_srgb::transparentWhite().toUint32() == rgba8_unorm::transparentWhite().toUint32());
	CHECK(rgba8_srgb(rgba8_unorm::opaqueWhite()) == rgba8_srgb::opaqueWhite());

	// Ensure the layout allows simple bit casting
	const uint32_t C4 = 0x12ABCDEF;

	// Valid for little-endian machines
	rgba8_unorm rgb4(C4);
	CHECK(rgb4.a == 0x12);
	CHECK(rgb4.b == 0xAB);
	CHECK(rgb4.g == 0xCD);
	CHECK(rgb4.r == 0xEF);
	CHECK(rgb4.toUint32() == C4);
	CHECK(std::bit_cast<uint32_t>(rgb4) == C4);
	CHECK(rgb4 == std::bit_cast<rgba8_unorm>(C4));
}

} // namespace m
