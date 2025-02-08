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
};

enum class LayoutXGravity {
	Left,
	Center,
	Right,
};

enum class LayoutYGravity {
	Top,
	Center,
	Bottom,
};

enum class LayoutSizingType {
	Fit,
	Grow,
	Percent,
};

struct LayoutSizing {
	constexpr static float NO_MAX = std::numeric_limits<float>::max();

	static LayoutSizing fit(float min = 0.0f, float max = NO_MAX) noexcept
	{
		return { LayoutSizingType::Fit, min, max };
	}

	static LayoutSizing grow(float min = 0.0f, float max = NO_MAX) noexcept
	{
		return { LayoutSizingType::Grow, min, max };
	}

	static LayoutSizing fixed(float value) noexcept { return { LayoutSizingType::Fit, value, value }; }

	static LayoutSizing percent(float value) noexcept
	{
		return { LayoutSizingType::Percent, value / 100.0f, value / 100.0f };
	}

	LayoutSizingType type = LayoutSizingType::Fit;
	float min = 0.0f;
	float max = NO_MAX;
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
	LayoutXGravity x_gravity = LayoutXGravity::Left;
	LayoutYGravity y_gravity = LayoutYGravity::Top;
	LayoutSizing x_sizing = {};
	LayoutSizing y_sizing = {};
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
	BorderSetup border = {};
	RectangleSetup rectangle = {};
};

struct ViewportSetup {
	std::u8string_view id = u8"";
	LayoutSetup layout = {};
	BorderSetup border = {};
	//RenderGraphReference render_graph;
};

struct LabelSetup {
	std::u8string_view label = u8"";
};

} // namespace voxen::gfx::ui
