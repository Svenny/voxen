#pragma once

#include <voxen/gfx/vk/render_graph.hpp>
#include <voxen/gfx/vk/render_graph_resource.hpp>
#include <voxen/os/os_fwd.hpp>

#include <memory>

namespace voxen::gfx::vk
{

class Device;

struct RenderGraphPrivate;

// Management class connecting render graphs subsystem with the rest of GFX module
class VOXEN_API RenderGraphRunner {
public:
	// Creates a swapchain attached to the window.
	// This window must not be in use by any other swapchain.
	RenderGraphRunner(Device &device, os::GlfwWindow &window);
	RenderGraphRunner(RenderGraphRunner &&) = delete;
	RenderGraphRunner(const RenderGraphRunner &) = delete;
	RenderGraphRunner &operator=(RenderGraphRunner &&) = delete;
	RenderGraphRunner &operator=(const RenderGraphRunner &) = delete;
	~RenderGraphRunner() noexcept;

	// Attach render graph to this executor.
	// This will detach the previous graph and trigger rebuild of the new one.
	void attachGraph(std::shared_ptr<IRenderGraph> graph);
	// Trigger rebuild of the currently attached render graph.
	// Do nothing if no graph is attached.
	void rebuildGraph();
	// Execute the currently attached render graph.
	// Do nothing if no graph is attached.
	void executeGraph();

private:
	Device &m_device;
	os::GlfwWindow &m_window;

	std::shared_ptr<RenderGraphPrivate> m_private;
	std::shared_ptr<IRenderGraph> m_graph;

	void finalizeRebuild();
	void publishResourceHandles();

	template<typename T>
	void visitCommand(RenderGraphExecution &exec, T &cmd);
};

} // namespace voxen::gfx::vk
