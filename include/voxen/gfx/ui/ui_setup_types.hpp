#pragma once

#include <voxen/util/packed_color.hpp>

#include <cstdint>
#include <limits>
#include <string_view>

namespace voxen::gfx::ui
{

// Controls how children are laid out in a container item
enum class LayoutDirection : uint8_t {
	// Children are laid out horizontally (in X axis), the first being the leftmost one.
	// Wrapping (multi-line layout) is not performed.
	LeftToRight,
	// Children are laid out vertically (in Y axis), the first being the topmost one.
	// Wrapping (multi-column layout) is not performed.
	TopToBottom,
	// Children are stacked (in Z axis), the first being the furthermost one.
	// Allows to create transparent overlays, e.g. in-game HUD over the viewport.
	BackToFront,
};

// Controls where children are "pushed" along one axis
enum class Gravity : uint8_t {
	// Pushed to the lower boundary (left for X, top for Y)
	Min,
	// Pushed to the center, makes free space evenly distributed at both boundaries
	Center,
	// Pushed to the higher boundary (right for X, bottom for Y)
	Max,
};

// Controls how an item is sized along one axis
enum class AxisSizingType : uint8_t {
	// Minimal size fitting inner content, within min/max limits.
	Fit,
	// Size is expanded to fill the whole parent container, within min/max limits.
	// Multiple items with this policy will attempt to evenly distribute free space.
	Grow,
	// Size is fixed and is independent of inner content or parent dimensions
	Fixed,
	// Size is automatically adjusted to a percentage of parent container size (minus padding)
	Percent,
};

// Controls item sizing along a single axis
struct AxisSizing {
	constexpr static float NO_MAX = std::numeric_limits<float>::max();

	static AxisSizing fit(float min = 0.0f, float max = NO_MAX) noexcept { return { AxisSizingType::Fit, min, max }; }

	static AxisSizing grow(float min = 0.0f, float max = NO_MAX) noexcept { return { AxisSizingType::Grow, min, max }; }

	static AxisSizing fixed(float value) noexcept { return { AxisSizingType::Fixed, value, value }; }

	// `value` is percents in [0..100] range
	static AxisSizing percent(float value) noexcept
	{
		return { AxisSizingType::Percent, value / 100.0f, value / 100.0f };
	}

	AxisSizingType type = AxisSizingType::Fit;
	float min = 0.0f;
	float max = NO_MAX;
};

// Controls item sizing along both axes
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

	// `x`, `y` are percents in [0..100] range
	static Sizing percent(float x, float y) noexcept { return { AxisSizing::percent(x), AxisSizing::percent(y) }; }

	AxisSizing x = {};
	AxisSizing y = {};
};

// Controls padding between container boundaries and its children items
struct Padding {
	static Padding all(uint8_t value) noexcept { return { value, value, value, value }; }

	uint8_t left = 0;
	uint8_t right = 0;
	uint8_t top = 0;
	uint8_t bottom = 0;
};

// Controls all layout aspects of a container item
struct Layout {
	LayoutDirection direction = LayoutDirection::LeftToRight;
	Padding padding = {};
	uint8_t child_gap = 0;
	Gravity x_gravity = Gravity::Min;
	Gravity y_gravity = Gravity::Min;
};

struct BorderSide {
	uint16_t width = 0;
	PackedColorSrgb color = { 0, 0, 0, 0 };
};

struct Border {
	BorderSide left = {};
	BorderSide right = {};
	BorderSide top = {};
	BorderSide bottom = {};
	BorderSide inner = {};
};

struct FillRectangle {
	PackedColorSrgb color = { 0, 0, 0, 0 };
};

// Arguments for `UiBuilder::div()`
struct DivSetup {
	std::u8string_view id = u8"";
	Layout layout = {};
	Sizing sizing = Sizing::fit();
	Border border = {};
	FillRectangle rectangle = {};
};

// Arguments for `UiBuilder::viewport()`
struct ViewportSetup {
	std::u8string_view id = u8"";
	Layout layout = {};
	Sizing sizing = Sizing::grow();
	Border border = {};
	// TODO: image resizing policy (letterbox/stretch/padding)
	// TODO: add render graph node references
	//RenderGraphReference render_graph;
};

// Arguments for `UiBuilder::text()`
struct TextSetup {
	std::u8string_view text = u8"";
	uint16_t font_id = 0;
	uint16_t font_size = 0;
	PackedColorSrgb color = { 0, 0, 0, 255 };
};

} // namespace voxen::gfx::ui
