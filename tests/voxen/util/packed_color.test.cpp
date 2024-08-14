#include <voxen/util/packed_color.hpp>

#include <catch2/catch.hpp>

namespace voxen
{

TEST_CASE("'PackedColor' sanity check", "[voxen::packed_color]")
{
	const glm::vec3 C1 = glm::vec3(20, 40, 50) / 255.0f;
	const glm::vec4 C2 = glm::vec4(4, 47, 240, 192) / 255.0f;
	const glm::vec3 C3 = glm::vec3(0.9f, 127.5f, 255.0f) / 255.0f;

	PackedColorSrgb srgb1(C1);
	PackedColorSrgb srgb2(C2);
	PackedColorSrgb srgb3(C3);

	// sRGB conversion and quantization are lossy, check representations manually
	CHECK(srgb1 == PackedColorSrgb { 79, 110, 122 });
	CHECK(srgb2 == PackedColorSrgb { 34, 119, 248, 192 });
	CHECK(srgb3 == PackedColorSrgb { 12, 188, 255 });

	// Round-trip check, must pass if the rounding is done properly
	CHECK(PackedColorSrgb(srgb1.toVec3()) == srgb1);
	CHECK(PackedColorSrgb(srgb2.toVec4()) == srgb2);
	CHECK(PackedColorSrgb(srgb3.toVec3()) == srgb3);

	PackedColorLinear rgb1(C1);
	PackedColorLinear rgb2(C2);
	PackedColorLinear rgb3(C3);

	CHECK(rgb1.toVec3() == C1);
	CHECK(rgb2.toVec4() == C2);
	// `rgb3` won't be equal to the original color
	// due to quantization, just check round-trip
	CHECK(PackedColorLinear(rgb3.toVec3()) == rgb3);

	// These trivial colors must have the same representation
	CHECK(PackedColorSrgb::opaqueBlack() == PackedColorLinear::opaqueBlack());
	CHECK(PackedColorSrgb::opaqueWhite() == PackedColorLinear::opaqueWhite());
	CHECK(PackedColorSrgb::transparentBlack() == PackedColorLinear::transparentBlack());
	CHECK(PackedColorSrgb::transparentWhite() == PackedColorLinear::transparentWhite());

	// Ensure the layout allows simple bit casting
	const uint32_t C4 = 0x12ABCDEF;

	// Valid for little-endian machines
	PackedColorLinear rgb4(C4);
	CHECK(rgb4.a == 0x12);
	CHECK(rgb4.b == 0xAB);
	CHECK(rgb4.g == 0xCD);
	CHECK(rgb4.r == 0xEF);
	CHECK(rgb4.toUint32() == C4);
	CHECK(std::bit_cast<uint32_t>(rgb4) == C4);
	CHECK(rgb4 == std::bit_cast<PackedColorLinear>(C4));
}

} // namespace voxen
