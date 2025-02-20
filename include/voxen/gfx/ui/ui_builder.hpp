#pragma once

#include <voxen/gfx/ui/ui_fwd.hpp>
#include <voxen/gfx/ui/ui_setup_types.hpp>
#include <voxen/visibility.hpp>

namespace voxen::gfx::ui
{

// Scoped object returned when a container item is added into `UiBuilder`.
// Items added further will be children of this item until the object goes out of scope.
// Store it only in local variables or you will get undefined behavior.
class VOXEN_API ScopedContainer {
public:
	// Internal constructor, use `UiBuilder` methods to construct this object
	explicit ScopedContainer(detail::UiSystemImpl &ui, detail::ContainerImpl &impl) noexcept : m_ui(ui), m_impl(impl) {}
	ScopedContainer(ScopedContainer &&) = delete;
	ScopedContainer(const ScopedContainer &) = delete;
	ScopedContainer &operator=(ScopedContainer &&) = delete;
	ScopedContainer &operator=(const ScopedContainer &) = delete;
	~ScopedContainer();

	// Whether mouse cursor is currently hovering over this item.
	// Takes hierarchy and overlapping items into account correctly.
	bool hovered() const noexcept;
	// Whether left mouse button is currently hovered AND pressed on this item.
	// Unlike `pressedThisFrame()` will return true as long as the button is pressed.
	bool pressed() const noexcept;
	// Whether left mouse button was pressed on this item in this exact frame.
	bool pressedThisFrame() const noexcept;
	// Whether left mouse button stopped being pressed on this item
	// in this exact frame while still being hovered over.
	// The latter condition allows the user to remove cursor from the item
	// while holding left button pressed to e.g. "cancel" clicking a button.
	bool releasedThisFrame() const noexcept;

	// Override fill color of this item. Used to select colors depending
	// on mouse state (hovered/clicked/etc.) for e.g. button elements.
	void setColor(PackedColorSrgb color) noexcept;

private:
	detail::UiSystemImpl &m_ui;
	detail::ContainerImpl &m_impl;
};

// Temporary helper to declare items during frame UI layout.
// Can be freely passed by reference between UI generating code,
// but must NOT be used after `UiSystem::endFrame()` is called.
// Like the whole UI system, this class is NOT thread-safe.
class VOXEN_API UiBuilder {
public:
	// Internal constructor, use `UiSystem` methods to construct this object
	explicit UiBuilder(detail::UiSystemImpl &ui_impl) noexcept : m_ui_impl(ui_impl) {}
	UiBuilder(UiBuilder &&) = delete;
	UiBuilder(const UiBuilder &) = delete;
	UiBuilder &operator=(UiBuilder &&) = delete;
	UiBuilder &operator=(const UiBuilder &) = delete;
	~UiBuilder() = default;

	// Open a new space dividing container item (named after similar HTML tag)
	// that can adjust size based on inner content and be filled with solid color.
	ScopedContainer div(DivSetup setup);
	// Open a new container item filled with image from a 3D viewport.
	// Image is produced by a render graph node.
	ScopedContainer viewport(ViewportSetup setup);

	// Create a text item with automatic layout/wrapping calculation
	void text(TextSetup setup);

private:
	detail::UiSystemImpl &m_ui_impl;
};

} // namespace voxen::gfx::ui
