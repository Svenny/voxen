#pragma once

#include <voxen/visibility.hpp>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <bit>

namespace voxen
{

// Packed sRGB-aware 8-bit RGBA color storage.
// Supports conversion between various representations
// and bit-casting to `uint32_t` for bulk memory operations.
// As usual with sRGB, alpha channel is always linear.
template<bool Linear>
struct VOXEN_API PackedColor {
	constexpr PackedColor() = default;
	// Directly construct from byte values without conversion
	constexpr PackedColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) noexcept : r(r), g(g), b(b), a(a) {}
	// Directly construct from packed `uint32_t` without conversion; endian-dependent
	constexpr PackedColor(uint32_t rgba) noexcept { *this = std::bit_cast<PackedColor>(rgba); }

	// Construct from linear [0; 1] RGB values (with sRGB conversion if !Linear); A will be 255
	PackedColor(const glm::vec3 &linear) noexcept;
	// Construct from linear [0; 1] RGBA values (with sRGB conversion if !Linear)
	PackedColor(const glm::vec4 &linear) noexcept;
	// Construct from a differently encoded value
	PackedColor(const PackedColor<!Linear> &other) noexcept;

	bool operator==(const PackedColor &) const = default;
	bool operator!=(const PackedColor &) const = default;

	// Pack to a single `uint32_t` without conversion; endian-dependent
	uint32_t toUint32() const noexcept { return std::bit_cast<uint32_t>(*this); }
	// Get linearized [0; 1] values of RGB components
	glm::vec3 toVec3() const noexcept;
	// Get linearized [0; 1] values of RGBA components
	glm::vec4 toVec4() const noexcept;

	// Get linearized value
	PackedColor<true> toLinear() const noexcept { return PackedColor<true>(*this); }
	// Get sRGB-encoded value
	PackedColor<false> toSrgb() const noexcept { return PackedColor<false>(*this); }

	constexpr static PackedColor opaqueBlack() noexcept { return { 0, 0, 0, 255 }; }
	constexpr static PackedColor transparentBlack() noexcept { return { 0, 0, 0, 0 }; }
	constexpr static PackedColor opaqueWhite() noexcept { return { 255, 255, 255, 255 }; }
	constexpr static PackedColor transparentWhite() noexcept { return { 255, 255, 255, 0 }; }

	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
};

// Templates are instantiated externally in `packed_color.cpp`
extern template struct PackedColor<true>;
extern template struct PackedColor<false>;

using PackedColorLinear = PackedColor<true>;
using PackedColorSrgb = PackedColor<false>;

} // namespace voxen
