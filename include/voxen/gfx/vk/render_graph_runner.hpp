#pragma once

#include <voxen/gfx/vk/render_graph.hpp>
#include <voxen/gfx/vk/render_graph_resource.hpp>

#include <memory>

namespace voxen::gfx::vk
{

struct RenderGraphPrivate;

// Management class connecting render graphs subsystem with the rest of GFX module
class VOXEN_API RenderGraphRunner {
public:
	RenderGraphRunner() noexcept;
	RenderGraphRunner(RenderGraphRunner &&) = delete;
	RenderGraphRunner(const RenderGraphRunner &) = delete;
	RenderGraphRunner &operator=(RenderGraphRunner &&) = delete;
	RenderGraphRunner &operator=(const RenderGraphRunner &) = delete;
	~RenderGraphRunner() noexcept;

	// Attach render graph to this executor.
	// This will destroy the previously attached graph and trigger rebuild of the new one.
	void attachGraph(std::unique_ptr<IRenderGraph> graph);
	// Trigger rebuild of the currently attached render graph.
	// Do nothing if no graph is attached.
	void rebuildGraph();
	// Execute the currently attached render graph.
	// Do nothing if no graph is attached.
	void executeGraph();

private:
	std::shared_ptr<RenderGraphPrivate> m_private;
	std::unique_ptr<IRenderGraph> m_graph;

	void finalizeRebuild();
	void publishResourceHandles();

	template<typename T>
	void visitCommand(RenderGraphExecution &exec, T &cmd);
};

} // namespace voxen::gfx::vk
