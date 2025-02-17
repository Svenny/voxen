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
	constexpr uint8_t FONT_SIZE = 44;

	constexpr voxen::PackedColorSrgb COLOR_BASE = { 80, 80, 80 };
	constexpr voxen::PackedColorSrgb COLOR_HOVERED = { 120, 120, 120 };
	constexpr voxen::PackedColorSrgb COLOR_PRESSED = { 50, 50, 50 };

	// Draw button background
	auto container = ui.div({
		.id = text,
		.layout = {
			.padding = LayoutPadding::all(INNER_PADDING),
			.x_gravity = Gravity::Min,
			.y_gravity = Gravity::Center,
		},
		.sizing = {
			.x = AxisSizing::grow(),
			.y = AxisSizing::fit(INNER_PADDING + INNER_PADDING + FONT_SIZE),
		},
		.rectangle = { .color = COLOR_BASE },
	});

	if (container.pressed()) {
		container.setColor(COLOR_HOVERED);
	} else if (container.hovered()) {
		container.setColor(COLOR_PRESSED);
	}

	// TODO: font size, wrapping, layout settings
	//ui.label({ .label = text });
	ui.div({
		.sizing = Sizing::fixed(static_cast<float>(text.size() * FONT_SIZE), FONT_SIZE),
		.rectangle = { .color = { 0, 0, 0, 255 } },
	});

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
	constexpr uint8_t VERSION_FONT_SIZE = 18;

	auto root = ui.div({
		.layout = {
			.direction = LayoutDirection::LeftToRight,
		},
		.sizing = Sizing::grow(),
		.rectangle = { .color = COLOR_BACKGROUND },
	});

	// Space buttons slightly apart from left border
	ui.div({
		.sizing = { .x = AxisSizing::percent(15.0f) },
	});

	{
		// Store all buttons in this div
		auto right_panel = ui.div({
			.layout = { .direction = LayoutDirection::TopToBottom },
			.sizing = Sizing::grow(),
			.rectangle = { .color = { 30, 20, 10, 255 } },
		});

		// Spacer to push buttons to the center
		ui.div({
			.sizing = Sizing::grow(),
			.rectangle = { .color = { 30, 70, 30, 255 } },
		});

		{
			auto buttons_div = ui.div({
				.layout = {
					.direction = LayoutDirection::TopToBottom,
					.child_gap = 16,
					.y_gravity = Gravity::Center,
				},
				.sizing = { .x = AxisSizing::fit(), .y = AxisSizing::grow() },
				.rectangle = { .color = { 200, 200, 200, 255 } },
			});

			if (mainMenuButton(ui, u8"Single player")) {
				// TODO: enter game
			}

			if (mainMenuButton(ui, u8"Multiplayer")) {
				// TODO: enter server search menu
			}

			if (mainMenuButton(ui, u8"Settings")) {
				// TODO: enter settings menu
			}

			if (mainMenuButton(ui, u8"Exit")) {
				// TODO: exit game
			}
		}

		{
			// One more spacer, also contains version string in the lower right corner
			auto bottom_div = ui.div({
				.layout = {
					.padding = { .right = 8, .bottom = 8 },
					.x_gravity = Gravity::Max,
					.y_gravity = Gravity::Max,
				},
				.sizing = Sizing::grow(),
				.rectangle = { .color = { 10, 20, 30, 255 } },
			});

			// TODO: font size, label
			//ui.label({ .label = m_version_string });
			ui.div({
				.sizing = Sizing::fixed(static_cast<float>(m_version_string.size() * VERSION_FONT_SIZE), VERSION_FONT_SIZE),
				.rectangle = { .color = { 0, 0, 0, 255 } },
			});
		}
	}
}

} // namespace vxgame
