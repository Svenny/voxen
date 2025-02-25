#include <voxen/gfx/ui/ui_builder.hpp>

#include "ui_system_impl.hpp"

namespace voxen::gfx::ui
{

// ScopedContainer

ScopedContainer::~ScopedContainer()
{
	m_ui.popContainer(m_impl);
}

bool ScopedContainer::hovered() const noexcept
{
	return m_impl.hovered;
}

bool ScopedContainer::pressed() const noexcept
{
	return m_impl.pressed;
}

bool ScopedContainer::pressedThisFrame() const noexcept
{
	return m_impl.pressed && m_impl.pressed_changed_this_frame;
}

bool ScopedContainer::releasedThisFrame() const noexcept
{
	return m_impl.hovered && !m_impl.pressed && m_impl.pressed_changed_this_frame;
}

void ScopedContainer::setColor(PackedColorSrgb color) noexcept
{
	m_impl.rectangle.color = color;
}

// UiBuilder

auto UiBuilder::div(DivSetup setup) -> ScopedContainer
{
	return ScopedContainer(m_ui_impl, m_ui_impl.pushContainer(setup));
}

void UiBuilder::text(TextSetup setup)
{
	div({
		.id = setup.text,
		.sizing = Sizing::fixed(static_cast<float>(setup.text.size() * setup.font_size), setup.font_size),
		.rectangle = { .color = setup.color },
	});
}

} // namespace voxen::gfx::ui
