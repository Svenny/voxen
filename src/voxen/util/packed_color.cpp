#include <voxen/util/packed_color.hpp>

#include <algorithm>
#include <cmath>

namespace voxen
{

namespace
{

uint8_t floatToU8Linear(float v) noexcept
{
	v = std::clamp(v, 0.0f, 1.0f);
	return uint8_t(v * 255.0f + 0.5f);
}

float u8LinearToFloat(uint8_t v) noexcept
{
	return float(v) / 255.0f;
}

// Khronos data format specification, sRGB EOTF^-1
uint8_t floatToU8Srgb(float v) noexcept
{
	if (v <= 0.0031308f) {
		return floatToU8Linear(v * 12.92f);
	} else {
		v = 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
		return floatToU8Linear(v);
	}
}

// Khronos data format specification, sRGB EOTF
float u8SrgbToFloat(uint8_t v) noexcept
{
	if (v <= uint8_t(0.04045f * 255.0f)) {
		return float(v) / (255.0f * 12.92f);
	} else {
		float f = (float(v) / 255.0f + 0.055f) / 1.055f;
		return std::pow(f, 2.4f);
	}
}

} // namespace

template<bool Linear>
PackedColor<Linear>::PackedColor(const glm::vec3 &linear) noexcept : a(255)
{
	if constexpr (Linear) {
		r = floatToU8Linear(linear.r);
		g = floatToU8Linear(linear.g);
		b = floatToU8Linear(linear.b);
	} else {
		r = floatToU8Srgb(linear.r);
		g = floatToU8Srgb(linear.g);
		b = floatToU8Srgb(linear.b);
	}
}

template<bool Linear>
PackedColor<Linear>::PackedColor(const glm::vec4 &linear) noexcept : a(floatToU8Linear(linear.a))
{
	if constexpr (Linear) {
		r = floatToU8Linear(linear.r);
		g = floatToU8Linear(linear.g);
		b = floatToU8Linear(linear.b);
	} else {
		r = floatToU8Srgb(linear.r);
		g = floatToU8Srgb(linear.g);
		b = floatToU8Srgb(linear.b);
	}
}

template<bool Linear>
PackedColor<Linear>::PackedColor(const PackedColor<!Linear> &other) noexcept : a(other.a)
{
	if constexpr (Linear) {
		r = floatToU8Linear(u8SrgbToFloat(other.r));
		g = floatToU8Linear(u8SrgbToFloat(other.g));
		b = floatToU8Linear(u8SrgbToFloat(other.b));
	} else {
		r = floatToU8Srgb(u8LinearToFloat(other.r));
		g = floatToU8Srgb(u8LinearToFloat(other.g));
		b = floatToU8Srgb(u8LinearToFloat(other.b));
	}
}

template<bool Linear>
glm::vec3 PackedColor<Linear>::toVec3() const noexcept
{
	if (Linear) {
		return { u8LinearToFloat(r), u8LinearToFloat(g), u8LinearToFloat(b) };
	} else {
		return { u8SrgbToFloat(r), u8SrgbToFloat(g), u8SrgbToFloat(b) };
	}
}

template<bool Linear>
glm::vec4 PackedColor<Linear>::toVec4() const noexcept
{
	if (Linear) {
		return { u8LinearToFloat(r), u8LinearToFloat(g), u8LinearToFloat(b), u8LinearToFloat(a) };
	} else {
		return { u8SrgbToFloat(r), u8SrgbToFloat(g), u8SrgbToFloat(b), u8LinearToFloat(a) };
	}
}

// External templates instantiation (declared in the header)
template struct PackedColor<true>;
template struct PackedColor<false>;

} // namespace voxen
