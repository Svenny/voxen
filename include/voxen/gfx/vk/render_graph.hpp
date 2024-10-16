#pragma once

#include <voxen/visibility.hpp>

namespace voxen::gfx::vk
{

class RenderGraphBuilder;
class RenderGraphExecution;

class VOXEN_API IRenderGraph {
public:
	IRenderGraph() = default;
	IRenderGraph(IRenderGraph &&) = default;
	IRenderGraph(const IRenderGraph &) = default;
	IRenderGraph &operator=(IRenderGraph &&) = default;
	IRenderGraph &operator=(const IRenderGraph &) = default;
	virtual ~IRenderGraph() noexcept;

	// Will be called when attaching a new render graph to an executor
	// or on explicit request (e.g. when static rendering options change).
	// Will not be called between `beginExecution()` and `endExecution()`.
	virtual void rebuild(RenderGraphBuilder &bld) = 0;

	// Notification about beginning of the graph execution.
	// Called before any render/compute pass callback but after starting recording the command buffer.
	virtual void beginExecution(RenderGraphExecution & /*exec*/) {}
	// Notification about ending of the graph execution.
	// Called after all render/compute pass callbacks but before stopping recording the command buffer.
	virtual void endExecution(RenderGraphExecution & /*exec*/) {}
};

} // namespace voxen::gfx::vk
