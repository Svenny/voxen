#include <voxen/gfx/vk/render_graph_runner.hpp>

#include <voxen/gfx/vk/render_graph_execution.hpp>

#include "render_graph_private.hpp"

namespace voxen::gfx::vk
{

RenderGraphRunner::RenderGraphRunner() noexcept = default;
RenderGraphRunner::~RenderGraphRunner() noexcept = default;

template<>
void RenderGraphRunner::visitCommand(RenderGraphExecution &exec, RenderGraphPrivate::BarrierCommand &cmd)
{
	// TODO: vkCmdPipelineBarrier
	(void) exec;
	(void) cmd;
}

template<>
void RenderGraphRunner::visitCommand(RenderGraphExecution &exec, RenderGraphPrivate::RenderPassCommand &cmd)
{
	// TODO: debug mark pass begin
	// TODO: vkCmdBeginRendering
	cmd.callback(*m_graph, exec);
	// TODO: vkCmdEndRendering
	// TODO: debug mark pass end
}

template<>
void RenderGraphRunner::visitCommand(RenderGraphExecution &exec, RenderGraphPrivate::ComputePassCommand &cmd)
{
	// TODO: debug mark pass begin
	cmd.callback(*m_graph, exec);
	// TODO: debug mark pass end
}

void RenderGraphRunner::attachGraph(std::unique_ptr<IRenderGraph> graph)
{
	// Drop all resources/commands for previous graph
	m_private.reset();
	m_graph = std::move(graph);

	rebuildGraph();
}

void RenderGraphRunner::rebuildGraph()
{
	if (!m_graph) {
		return;
	}

	// Drop all previously created resources/commands
	m_private = std::make_shared<RenderGraphPrivate>();

	RenderGraphBuilder bld(*m_private);
	m_graph->rebuild(bld);
}

void RenderGraphRunner::executeGraph()
{
	assert(m_graph);

	// TODO: begin command buffer recording
	RenderGraphExecution exec(*m_private);

	publishResourceHandles();

	m_graph->beginExecution(exec);

	for (auto &cmd : m_private->commands) {
		// Dispatch to type-specific command handlers
		std::visit([&](auto &&arg) { visitCommand(exec, arg); }, cmd);
	}

	m_graph->endExecution(exec);

	// TODO: end command buffer recording, submit it
}

void RenderGraphRunner::finalizeRebuild()
{
	// Normalize create infos for double-buffered resources
	for (auto &image : m_private->images) {
		if (!image.temporal_sibling) {
			continue;
		}

		image.create_info.usage |= image.temporal_sibling->create_info.usage;

		for (auto &view : image.views) {
			if (!view.temporal_sibling) {
				continue;
			}

			view.usage_create_info.usage |= view.temporal_sibling->usage_create_info.usage;
		}
	}

	// Allocate resources (except dynamic-sized buffers)
	// TODO: allocate buffers
	// TODO: allocate images
	// TODO: create image views
}

void RenderGraphRunner::publishResourceHandles()
{
	for (auto &buffer : m_private->buffers) {
		VkBuffer handle = buffer.handle;

		if (buffer.dynamic_sized) {
			// Publish NULL handle to prevent using this buffer without providing the size.
			// Actual handle will be published by `setDynamicBufferSize()`.
			buffer.used_size = 0;
			handle = VK_NULL_HANDLE;
		}

		if (buffer.resource) {
			buffer.resource->setHandle(handle);
		}
	}

	for (auto &image : m_private->images) {
		// Swap handles of double-buffered images. Compare pointers to swap only once.
		if (image.temporal_sibling && (&image < image.temporal_sibling)) {
			std::swap(image.handle, image.temporal_sibling->handle);
		}

		if (image.resource) {
			image.resource->setHandle(image.handle);
		}

		for (auto &view : image.views) {
			// Swap handles of double-buffered views. Compare pointers to swap only once.
			if (view.temporal_sibling && (&view < view.temporal_sibling)) {
				std::swap(view.handle, view.temporal_sibling->handle);
			}

			if (view.resource) {
				view.resource->setHandle(view.handle);
			}
		}
	}
}

} // namespace voxen::gfx::vk
