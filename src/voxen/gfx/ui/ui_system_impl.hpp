#pragma once

#include <voxen/gfx/ui/ui_render_data.hpp>
#include <voxen/gfx/ui/ui_setup_types.hpp>
#include <voxen/gfx/ui/ui_system.hpp>

#include "ui_render_list_builder.hpp"

#include <forward_list>

namespace voxen::gfx::ui::detail
{

struct ContainerImpl {
	ContainerImpl *parent = nullptr;
	uint64_t id = 0;

	Layout layout = {};
	Sizing sizing = {};
	Border border = {};
	FillRectangle rectangle = {};

	bool hovered = false;
	bool pressed = false;
	bool pressed_changed_this_frame = false;

	float x = 0.0f;
	float y = 0.0f;

	float width = 0.0f;
	float height = 0.0f;

	std::vector<ContainerImpl *> children;
};

struct ContainerGhost {
	uint64_t id;
	float x, y, width, height;
	bool pressed;

	bool operator<(const ContainerGhost &other) noexcept { return id < other.id; }
	bool operator==(const ContainerGhost &other) noexcept { return id == other.id; }
};

class UiSystemImpl {
public:
	UiSystemImpl() { m_root_container.layout.direction = LayoutDirection::BackToFront; }

	ContainerImpl &pushContainer(DivSetup setup);
	void popContainer(ContainerImpl &container);

	void beginFrame(UiSystem::PerFrameData per_frame);
	RenderData endFrame();

private:
	ContainerImpl m_root_container;
	ContainerImpl *m_container_stack_top = &m_root_container;
	std::forward_list<ContainerImpl> m_containers;

	UiSystem::PerFrameData m_this_frame_data;
	UiSystem::PerFrameData m_prev_frame_data;

	std::vector<ContainerGhost> m_prev_frame_containers;

	RenderListBuilder m_render_list_builder;
};

} // namespace voxen::gfx::ui::detail
