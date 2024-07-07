#pragma once

#include <voxen/gfx/vk/render_graph_resource.hpp>

namespace voxen::gfx::vk
{

class FrameContext;

struct RenderGraphPrivate;

// A temporary entity passed to `IRenderGraph` execution callbacks.
// Use its interface to set dynamic parameters and get temporary objects,
// including the command buffer to record Vulkan commands into.
class VOXEN_API RenderGraphExecution {
public:
	explicit RenderGraphExecution(RenderGraphPrivate &priv) noexcept;
	RenderGraphExecution(RenderGraphExecution &&) = delete;
	RenderGraphExecution(const RenderGraphExecution &) = delete;
	RenderGraphExecution &operator=(RenderGraphExecution &&) = delete;
	RenderGraphExecution &operator=(const RenderGraphExecution &) = delete;
	~RenderGraphExecution() noexcept;

	// Set size of a dynamic-sized buffer.
	// This buffer must have been created by `RenderGraphBuilder`
	// during the last graph rebuild and have dynamic sizing.
	// It's not allowed to set size multiple times during one execution.
	void setDynamicBufferSize(RenderGraphBuffer &buffer, VkDeviceSize size);

	// Frame context that will execute this
	// Its command buffer is in recording state, do not end it manually.
	FrameContext &frameContext() noexcept { return m_frame_context; }


private:
	RenderGraphPrivate &m_private;
	FrameContext &m_frame_context;
};

} // namespace voxen::gfx::vk
