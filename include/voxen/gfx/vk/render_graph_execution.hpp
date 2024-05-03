#pragma once

#include <voxen/gfx/vk/render_graph_resource.hpp>

namespace voxen::gfx::vk
{

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

	// Command buffer to record graph execution commands into.
	// It is in recording state. Do not end this command buffer.
	VkCommandBuffer commandBuffer() const noexcept { return m_cmd_buffer; }

private:
	RenderGraphPrivate &m_private;

	VkCommandBuffer m_cmd_buffer = VK_NULL_HANDLE;
};

} // namespace voxen::gfx::vk
