#pragma once

#include <voxen/client/vulkan/common.hpp>
#include <voxen/client/window.hpp>
#include <voxen/common/gameview.hpp>
#include <voxen/common/world_state.hpp>

#include <string_view>

namespace voxen::client::vulkan
{

class Capabilities;
class DescriptorManager;
class DescriptorSetLayoutCollection;
class Device;
class DeviceAllocator;
class FramebufferCollection;
class Instance;
class MainLoop;
class PhysicalDevice;
class PipelineCache;
class PipelineCollection;
class PipelineLayoutCollection;
class ShaderModuleCollection;
class Surface;
class Swapchain;
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
		SurfaceLost,
		SwapchainOutOfDate,
	};

	bool start(Window &window) noexcept;
	void stop() noexcept;

	bool drawFrame(const WorldState &state, const GameView &view) noexcept;

	bool recreateSurface(Window &window) noexcept;
	bool recreateSwapchain(Window &window) noexcept;

	State state() const noexcept { return m_state; }

	Capabilities &capabilities() noexcept { return *m_capabilities; }
	const Capabilities &capabilities() const noexcept { return *m_capabilities; }
	Instance &instance() noexcept { return *m_instance; }
	const Instance &instance() const noexcept { return *m_instance; }
	PhysicalDevice &physicalDevice() noexcept { return *m_physical_device; }
	const PhysicalDevice &physicalDevice() const noexcept { return *m_physical_device; }
	Device &device() noexcept { return *m_device; }
	const Device &device() const noexcept { return *m_device; }

	DeviceAllocator &deviceAllocator() noexcept { return *m_device_allocator; }
	const DeviceAllocator &deviceAllocator() const noexcept { return *m_device_allocator; }
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
	DescriptorManager &descriptorManager() noexcept { return *m_descriptor_manager; }
	const DescriptorManager &descriptorManager() const noexcept { return *m_descriptor_manager; }

	Surface &surface() noexcept { return *m_surface; }
	const Surface &surface() const noexcept { return *m_surface; }
	PipelineCollection &pipelineCollection() noexcept { return *m_pipeline_collection; }
	const PipelineCollection &pipelineCollection() const noexcept { return *m_pipeline_collection; }

	Swapchain &swapchain() noexcept { return *m_swapchain; }
	const Swapchain &swapchain() const noexcept { return *m_swapchain; }
	FramebufferCollection &framebufferCollection() noexcept { return *m_framebuffer_collection; }
	const FramebufferCollection &framebufferCollection() const noexcept { return *m_framebuffer_collection; }

	MainLoop &mainLoop() noexcept { return *m_main_loop; }
	const MainLoop &mainLoop() const noexcept { return *m_main_loop; }
	TerrainRenderer &terrainRenderer() noexcept { return *m_terrain_renderer; }
	const TerrainRenderer &terrainRenderer() const noexcept { return *m_terrain_renderer; }

	bool loadInstanceLevelApi(VkInstance instance) noexcept;
	void unloadInstanceLevelApi() noexcept;
	bool loadDeviceLevelApi(VkDevice device) noexcept;
	void unloadDeviceLevelApi() noexcept;

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

	Capabilities *m_capabilities = nullptr;
	Instance *m_instance = nullptr;
	PhysicalDevice *m_physical_device = nullptr;
	Device *m_device = nullptr;

	DeviceAllocator *m_device_allocator = nullptr;
	TransferManager *m_transfer_manager = nullptr;
	TerrainSynchronizer *m_terrain_synchronizer = nullptr;

	ShaderModuleCollection *m_shader_module_collection = nullptr;
	PipelineCache *m_pipeline_cache = nullptr;
	DescriptorSetLayoutCollection *m_descriptor_set_layout_collection = nullptr;
	PipelineLayoutCollection *m_pipeline_layout_collection = nullptr;
	DescriptorManager *m_descriptor_manager = nullptr;

	Surface *m_surface = nullptr;
	PipelineCollection *m_pipeline_collection = nullptr;

	Swapchain *m_swapchain = nullptr;
	FramebufferCollection *m_framebuffer_collection = nullptr;

	MainLoop *m_main_loop = nullptr;
	TerrainRenderer *m_terrain_renderer = nullptr;

	static constinit Backend s_instance;

	static std::string_view stateToString(State state) noexcept;

	bool loadPreInstanceApi() noexcept;

	enum class StartStopMode {
		Everything,
		SurfaceDependentOnly,
		SwapchainDependentOnly
	};

	bool doStart(Window &window, StartStopMode mode) noexcept;
	void doStop(StartStopMode mode) noexcept;

	constexpr Backend(Impl &impl) noexcept;
	Backend(Backend &&) = delete;
	Backend(const Backend &) = delete;
	Backend &operator=(Backend &&) = delete;
	Backend &operator=(const Backend &) = delete;
	~Backend() noexcept;
};

} // namespace voxen::client::vulkan
