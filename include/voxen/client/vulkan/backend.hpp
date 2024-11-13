#pragma once

#include <voxen/common/gameview.hpp>
#include <voxen/common/world_state.hpp>
#include <voxen/gfx/gfx_fwd.hpp>
#include <voxen/gfx/vk/vk_include.hpp>
#include <voxen/os/glfw_window.hpp>
#include <voxen/svc/svc_fwd.hpp>

#include <extras/dyn_array.hpp>

#include <string_view>

namespace voxen::gfx::vk
{

class LegacyRenderGraph;

} // namespace voxen::gfx::vk

namespace voxen::client::vulkan
{

class DescriptorSetLayoutCollection;
class PipelineCache;
class PipelineCollection;
class PipelineLayoutCollection;
class ShaderModuleCollection;
class TerrainRenderer;
class TerrainSynchronizer;
class TransferManager;

class Backend {
public:
	struct Impl;

	enum class State {
		NotStarted,
		Started,
		Broken,
	};

	bool start(os::GlfwWindow &window, svc::ServiceLocator &svc) noexcept;
	void stop() noexcept;

	bool drawFrame(const WorldState &state, const GameView &view) noexcept;

	State state() const noexcept { return m_state; }

	gfx::GfxSystem &gfxSystem() noexcept { return *m_gfx_system; }
	gfx::vk::Instance &instance() noexcept { return *m_instance; }
	const gfx::vk::Instance &instance() const noexcept { return *m_instance; }
	gfx::vk::Device &device() noexcept { return *m_device; }
	const gfx::vk::Device &device() const noexcept { return *m_device; }

	TransferManager &transferManager() noexcept { return *m_transfer_manager; }
	const TransferManager &transferManager() const noexcept { return *m_transfer_manager; }
	TerrainSynchronizer &terrainSynchronizer() noexcept { return *m_terrain_synchronizer; }
	const TerrainSynchronizer &terrainSynchronizer() const noexcept { return *m_terrain_synchronizer; }

	ShaderModuleCollection &shaderModuleCollection() noexcept { return *m_shader_module_collection; }
	const ShaderModuleCollection &shaderModuleCollection() const noexcept { return *m_shader_module_collection; }
	PipelineCache &pipelineCache() noexcept { return *m_pipeline_cache; }
	const PipelineCache &pipelineCache() const noexcept { return *m_pipeline_cache; }
	DescriptorSetLayoutCollection &descriptorSetLayoutCollection() noexcept
	{
		return *m_descriptor_set_layout_collection;
	}
	const DescriptorSetLayoutCollection &descriptorSetLayoutCollection() const noexcept
	{
		return *m_descriptor_set_layout_collection;
	}
	PipelineLayoutCollection &pipelineLayoutCollection() noexcept { return *m_pipeline_layout_collection; }
	const PipelineLayoutCollection &pipelineLayoutCollection() const noexcept { return *m_pipeline_layout_collection; }

	gfx::vk::LegacyRenderGraph &renderGraph() noexcept { return *m_render_graph; }
	const gfx::vk::LegacyRenderGraph &renderGraph() const noexcept { return *m_render_graph; }
	gfx::vk::RenderGraphRunner &renderGraphRunner() noexcept { return *m_render_graph_runner; }
	const gfx::vk::RenderGraphRunner &renderGraphRunner() const noexcept { return *m_render_graph_runner; }
	PipelineCollection &pipelineCollection() noexcept { return *m_pipeline_collection; }
	const PipelineCollection &pipelineCollection() const noexcept { return *m_pipeline_collection; }

	TerrainRenderer &terrainRenderer() noexcept { return *m_terrain_renderer; }
	const TerrainRenderer &terrainRenderer() const noexcept { return *m_terrain_renderer; }

	// Declare pointers to Vulkan API entry points, moved
	// into a separate file because of size and ugliness
#include "api_table_declare.in"

	// Backend is the only Vulkan-related class designed
	// to be a singleton. The main arguments for this decision:
	// 1. Launching multiple backends makes no sense
	// 2. With non-singleton design each downstream entity would have to store reference
	//    to its backend, needlessly (because of p.1) increasing objects' and code size
	// 3. There is a lot of downstream entities (which strengthens p.2)
	static Backend &backend() noexcept { return s_instance; }
	static const Backend &cbackend() noexcept { return s_instance; }

private:
	State m_state = State::NotStarted;

	Impl &m_impl;

	gfx::GfxSystem *m_gfx_system = nullptr;
	gfx::vk::Instance *m_instance = nullptr;
	gfx::vk::Device *m_device = nullptr;

	TransferManager *m_transfer_manager = nullptr;
	TerrainSynchronizer *m_terrain_synchronizer = nullptr;

	ShaderModuleCollection *m_shader_module_collection = nullptr;
	PipelineCache *m_pipeline_cache = nullptr;
	DescriptorSetLayoutCollection *m_descriptor_set_layout_collection = nullptr;
	PipelineLayoutCollection *m_pipeline_layout_collection = nullptr;

	std::shared_ptr<gfx::vk::LegacyRenderGraph> m_render_graph;
	gfx::vk::RenderGraphRunner *m_render_graph_runner = nullptr;
	PipelineCollection *m_pipeline_collection = nullptr;

	TerrainRenderer *m_terrain_renderer = nullptr;

	static constinit Backend s_instance;

	static std::string_view stateToString(State state) noexcept;

	bool loadPreInstanceApi() noexcept;
	bool loadInstanceLevelApi(VkInstance instance) noexcept;
	void unloadInstanceLevelApi() noexcept;
	bool loadDeviceLevelApi(VkDevice device) noexcept;
	void unloadDeviceLevelApi() noexcept;

	bool doStart(os::GlfwWindow &window, svc::ServiceLocator &svc) noexcept;
	void doStop() noexcept;

	constexpr Backend(Impl &impl) noexcept;
	Backend(Backend &&) = delete;
	Backend(const Backend &) = delete;
	Backend &operator=(Backend &&) = delete;
	Backend &operator=(const Backend &) = delete;
	~Backend() noexcept;
};

} // namespace voxen::client::vulkan
