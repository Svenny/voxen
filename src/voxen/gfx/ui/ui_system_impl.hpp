#pragma once

#include <voxen/common/scratch_memory_allocator.hpp>
#include <voxen/common/scratch_memory_containers.hpp>
#include <voxen/gfx/ui/ui_render_data.hpp>
#include <voxen/gfx/ui/ui_setup_types.hpp>
#include <voxen/gfx/ui/ui_system.hpp>

namespace voxen::gfx::ui::detail
{

struct ContainerImpl {
	ContainerImpl(ScratchMemoryAllocatorScope &scratch) noexcept : children(scratch) {}

	~ContainerImpl()
	{
		// XXX: there is nothing to destroy, everything is scratch-allocated
		for (ContainerImpl *child : children) {
			child->~ContainerImpl();
		}
	}

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

	scratch_vector<ContainerImpl *> children;
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
	ContainerImpl &pushContainer(DivSetup setup);
	void popContainer(ContainerImpl &container);

	void beginFrame(ScratchMemoryAllocatorScope &scratch, UiSystem::PerFrameData per_frame);
	RenderData endFrame();

private:
	ScratchMemoryAllocatorScope *m_scratch = nullptr;
	scratch_unique_ptr<ContainerImpl> m_root_container;
	ContainerImpl *m_container_stack_top = nullptr;

	UiSystem::PerFrameData m_this_frame_data;
	UiSystem::PerFrameData m_prev_frame_data;

	std::vector<ContainerGhost> m_prev_frame_containers;
	size_t m_prev_frame_render_data_rectangles = 0;
};

} // namespace voxen::gfx::ui::detail
