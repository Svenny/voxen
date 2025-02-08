#include "game_ui.hpp"

#include <voxen/version.hpp>

#include <extras/string_utils.hpp>

#include <fmt/format.h>
#include <fmt/xchar.h>

namespace vxgame
{

using namespace voxen::gfx::ui;

namespace
{

bool mainMenuButton(UiBuilder &ui, std::u8string_view text)
{
	constexpr uint8_t INNER_PADDING = 6;
	constexpr uint8_t FONT_SIZE = 36;

	constexpr voxen::PackedColorSrgb COLOR_BASE = { 80, 80, 80 };
	constexpr voxen::PackedColorSrgb COLOR_HOVERED = { 120, 120, 120 };
	constexpr voxen::PackedColorSrgb COLOR_PRESSED = { 50, 50, 50 };

	// Draw button background
	auto container = ui.div({
		.id = text,
		.layout = {
			.padding = LayoutPadding::all(INNER_PADDING),
			.x_gravity = LayoutXGravity::Center,
			.y_gravity = LayoutYGravity::Center,
			.x_sizing = LayoutSizing::grow(),
			.y_sizing = LayoutSizing::fit(INNER_PADDING + INNER_PADDING + FONT_SIZE),
		},
		.rectangle = { .color = COLOR_BASE },
	});

	if (container.pressed()) {
		container.setColor(COLOR_HOVERED);
	} else if (container.hovered()) {
		container.setColor(COLOR_PRESSED);
	}

	// TODO: font size, wrapping, layout settings
	ui.label({ .label = text });

	return container.released();
}

} // namespace

Ui::Ui()
{
	m_version_string = fmt::format(u8"Voxen Sample Game v{}", extras::ascii_as_utf8(voxen::Version::STRING));
}

void Ui::draw(UiBuilder &ui)
{
	constexpr voxen::PackedColorSrgb COLOR_BACKGROUND = { 40, 40, 40 };

	auto root = ui.div({
		.layout = { .direction = LayoutDirection::LeftToRight },
		.rectangle = { .color = COLOR_BACKGROUND },
	});

	// Space buttons slightly apart from left border
	ui.div({
		.layout = { .x_sizing = LayoutSizing::percent(20.0f) },
	});

	{
		// Store all buttons in this div
		auto buttons_div = ui.div({
			.layout = {
				.direction = LayoutDirection::TopToBottom,
				.child_gap = 16,
				.x_gravity = LayoutXGravity::Left,
				.y_gravity = LayoutYGravity::Center,
				.y_sizing = LayoutSizing::grow(),
			},
		});

		ui.div({
			.layout = { .y_sizing = LayoutSizing::grow() },
		});

		if (mainMenuButton(ui, u8"Single player")) {
			// TODO: enter game
		}

		if (mainMenuButton(ui, u8"Settings")) {
			// TODO: enter settings menu
		}

		if (mainMenuButton(ui, u8"Exit")) {
			// TODO: exit game
		}

		{
			auto bottom_div = ui.div({
				.layout = {
					.padding = { .right = 8, .bottom = 8 },
					.x_gravity = LayoutXGravity::Right,
					.y_gravity = LayoutYGravity::Bottom,
					.x_sizing = LayoutSizing::grow(),
					.y_sizing = LayoutSizing::grow(),
				},
			});

			// TODO: font size
			ui.label({ .label = m_version_string });
		}
	}
}

} // namespace vxgame
