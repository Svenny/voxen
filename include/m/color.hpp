#pragma once

#include <m/packing.hpp>
#include <m/vector.hpp>

#include <bit>
#include <cmath>
#include <cstdint>

// 8-bit RGBA colors as in GPU formats `R8G8B8A8_UNORM` (`rgba8_unorm`) and `R8G8B8A8_SRGB`
// (`rgba8_srgb`). The layout allows bit-casting to `uint32_t` (endian-dependent) for bulk
// memory operations. Float vectors are always linear [0, 1] values, `rgba8_srgb` applies
// sRGB encoding to RGB components on conversion. As usual with sRGB, alpha is always linear.
//
// sRGB conversions are runtime-only (they need `pow`), everything else is `constexpr`.

namespace m
{

namespace detail
{

// Khronos data format specification, sRGB EOTF^-1
inline float srgbEncode(float linear) noexcept
{
	if (linear <= 0.0031308f) {
		return linear * 12.92f;
	}

	return 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
}

// Khronos data format specification, sRGB EOTF
inline float srgbDecode(float encoded) noexcept
{
	if (encoded <= 0.04045f) {
		return encoded / 12.92f;
	}

	return std::pow((encoded + 0.055f) / 1.055f, 2.4f);
}

} // namespace detail

template<bool Srgb>
struct rgba8 {
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
	uint8_t a = 0;

	constexpr rgba8() noexcept = default;
	// Take encoded byte values as is
	constexpr rgba8(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) noexcept : r(r), g(g), b(b), a(a) {}
	// Bit-cast from packed `uint32_t`; endian-dependent
	constexpr explicit rgba8(uint32_t packed) noexcept : rgba8(std::bit_cast<rgba8>(packed)) {}

	// Encode linear [0, 1] RGB values, alpha will be 255
	constexpr explicit rgba8(const float3 &linear) noexcept : rgba8(float4(linear, 1.0f)) {}
	// Encode linear [0, 1] RGBA values
	constexpr explicit rgba8(const float4 &linear) noexcept
		: r(encode(linear.x)), g(encode(linear.y)), b(encode(linear.z)), a(packUnorm<uint8_t>(linear.w))
	{}
	// Re-encode a color from the other encoding
	explicit rgba8(const rgba8<!Srgb> &other) noexcept : rgba8(other.toFloat4()) {}

	constexpr bool operator==(const rgba8 &) const noexcept = default;

	// Bit-cast to packed `uint32_t`; endian-dependent
	constexpr uint32_t toUint32() const noexcept { return std::bit_cast<uint32_t>(*this); }
	// Decode to linear [0, 1] RGB values
	constexpr float3 toFloat3() const noexcept { return float3(decode(r), decode(g), decode(b)); }
	// Decode to linear [0, 1] RGBA values
	constexpr float4 toFloat4() const noexcept { return float4(toFloat3(), unpackUnorm<float>(a)); }

	constexpr static rgba8 opaqueBlack() noexcept { return { 0, 0, 0, 255 }; }
	constexpr static rgba8 transparentBlack() noexcept { return { 0, 0, 0, 0 }; }
	constexpr static rgba8 opaqueWhite() noexcept { return { 255, 255, 255, 255 }; }
	constexpr static rgba8 transparentWhite() noexcept { return { 255, 255, 255, 0 }; }

private:
	constexpr static uint8_t encode(float linear) noexcept
	{
		if constexpr (Srgb) {
			return packUnorm<uint8_t>(detail::srgbEncode(linear));
		} else {
			return packUnorm<uint8_t>(linear);
		}
	}

	constexpr static float decode(uint8_t value) noexcept
	{
		if constexpr (Srgb) {
			return detail::srgbDecode(unpackUnorm<float>(value));
		} else {
			return unpackUnorm<float>(value);
		}
	}
};

using rgba8_unorm = rgba8<false>;
using rgba8_srgb = rgba8<true>;

} // namespace m
