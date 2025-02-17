#pragma once

#include <voxen/gfx/gfx_fwd.hpp>
#include <voxen/gfx/ui/ui_fwd.hpp>
#include <voxen/gfx/ui/ui_setup_types.hpp>
#include <voxen/visibility.hpp>

#include <extras/pimpl.hpp>

// TODO: cutting through API abstraction
using VkCommandBuffer = struct VkCommandBuffer_T *;

namespace voxen::gfx::ui
{

class VOXEN_API ScopedContainer {
public:
	explicit ScopedContainer(detail::UiBuilderImpl &ui, detail::ContainerImpl &impl) noexcept : m_ui(ui), m_impl(impl) {}
	ScopedContainer(ScopedContainer &&) = delete;
	ScopedContainer(const ScopedContainer &) = delete;
	ScopedContainer &operator=(ScopedContainer &&) = delete;
	ScopedContainer &operator=(const ScopedContainer &) = delete;
	~ScopedContainer();

	bool hovered() const noexcept;
	bool pressed() const noexcept;
	bool released() const noexcept;

	void setColor(PackedColorSrgb color) noexcept;

private:
	detail::UiBuilderImpl &m_ui;
	detail::ContainerImpl &m_impl;
};

class VOXEN_API UiBuilder {
public:
	UiBuilder();
	UiBuilder(UiBuilder &&) = delete;
	UiBuilder(const UiBuilder &) = delete;
	UiBuilder &operator=(UiBuilder &&) = delete;
	UiBuilder &operator=(const UiBuilder &) = delete;
	~UiBuilder();

	ScopedContainer div(DivSetup setup);
	ScopedContainer viewport(ViewportSetup setup);

	void label(LabelSetup setup);

	void render(int32_t width, int32_t height, GfxSystem &gfx, VkCommandBuffer cmd_buf);

private:
	extras::pimpl<detail::UiBuilderImpl, 512, 8> m_impl;
};

} // namespace voxen::gfx::ui
