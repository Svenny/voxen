#pragma once

#include <voxen/util/packed_color.hpp>

#include <cstdint>
#include <limits>
#include <string_view>

namespace voxen::gfx::ui
{

enum class LayoutDirection {
	LeftToRight,
	TopToBottom,
	BackToFront,
};

enum class Gravity {
	Min,
	Center,
	Max,
};

enum class AxisSizingType {
	Fit,
	Grow,
	Fixed,
	Percent,
};

struct AxisSizing {
	constexpr static float NO_MAX = std::numeric_limits<float>::max();

	static AxisSizing fit(float min = 0.0f, float max = NO_MAX) noexcept { return { AxisSizingType::Fit, min, max }; }

	static AxisSizing grow(float min = 0.0f, float max = NO_MAX) noexcept { return { AxisSizingType::Grow, min, max }; }

	static AxisSizing fixed(float value) noexcept { return { AxisSizingType::Fixed, value, value }; }

	static AxisSizing percent(float value) noexcept
	{
		return { AxisSizingType::Percent, value / 100.0f, value / 100.0f };
	}

	AxisSizingType type = AxisSizingType::Fit;
	float min = 0.0f;
	float max = NO_MAX;
};

struct Sizing {
	constexpr static float NO_MAX = AxisSizing::NO_MAX;

	static Sizing fit(float min = 0.0f, float max = NO_MAX) noexcept
	{
		return { AxisSizing::fit(min, max), AxisSizing::fit(min, max) };
	}

	static Sizing grow(float min = 0.0f, float max = NO_MAX) noexcept
	{
		return { AxisSizing::grow(min, max), AxisSizing::grow(min, max) };
	}

	static Sizing fixed(float width, float height) noexcept
	{
		return { AxisSizing::fixed(width), AxisSizing::fixed(height) };
	}

	static Sizing percent(float x, float y) noexcept { return { AxisSizing::percent(x), AxisSizing::percent(y) }; }

	AxisSizing x = {};
	AxisSizing y = {};
};

struct LayoutPadding {
	static LayoutPadding all(uint8_t value) noexcept { return { value, value, value, value }; }

	uint8_t left = 0;
	uint8_t right = 0;
	uint8_t top = 0;
	uint8_t bottom = 0;
};

struct LayoutSetup {
	LayoutDirection direction = LayoutDirection::LeftToRight;
	LayoutPadding padding = {};
	uint8_t child_gap = 0;
	Gravity x_gravity = Gravity::Min;
	Gravity y_gravity = Gravity::Min;
};

struct BorderSideSetup {
	float width = 0.0f;
	PackedColorSrgb color = { 0, 0, 0, 0 };
};

struct BorderSetup {
	BorderSideSetup left = {};
	BorderSideSetup right = {};
	BorderSideSetup top = {};
	BorderSideSetup bottom = {};
	BorderSideSetup inner = {};
};

struct RectangleSetup {
	PackedColorSrgb color = { 0, 0, 0, 0 };
};

struct DivSetup {
	std::u8string_view id = u8"";
	LayoutSetup layout = {};
	Sizing sizing = Sizing::fit();
	BorderSetup border = {};
	RectangleSetup rectangle = {};
};

struct ViewportSetup {
	std::u8string_view id = u8"";
	LayoutSetup layout = {};
	Sizing sizing = Sizing::grow();
	BorderSetup border = {};
	//RenderGraphReference render_graph;
};

struct LabelSetup {
	std::u8string_view label = u8"";
};

} // namespace voxen::gfx::ui
