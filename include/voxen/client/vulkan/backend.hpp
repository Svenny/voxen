#pragma once

#include <voxen/client/vulkan/common.hpp>
#include <voxen/client/window.hpp>
#include <voxen/common/gameview.hpp>
#include <voxen/common/world_state.hpp>

#include <string_view>

namespace voxen::client::vulkan
{

class AlgoDebugOctree;
class Device;
class DeviceAllocator;
class FramebufferCollection;
class Instance;
class MainLoop;
class PhysicalDevice;
class PipelineCache;
class PipelineCollection;
class PipelineLayoutCollection;
class RenderPassCollection;
class ShaderModuleCollection;
class Surface;
class Swapchain;
class TransferManager;

class Backend {
public:
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

	Instance *instance() const noexcept { return m_instance; }
	PhysicalDevice *physicalDevice() const noexcept { return m_physical_device; }
	Device *device() const noexcept { return m_device; }
	DeviceAllocator *deviceAllocator() const noexcept { return m_device_allocator; }
	TransferManager *transferManager() const noexcept { return m_transfer_manager; }
	ShaderModuleCollection *shaderModuleCollection() const noexcept { return m_shader_module_collection; }
	PipelineCache *pipelineCache() const noexcept { return m_pipeline_cache; }
	PipelineLayoutCollection *pipelineLayoutCollection() const noexcept { return m_pipeline_layout_collection; }
	Surface *surface() const noexcept { return m_surface; }
	RenderPassCollection *renderPassCollection() const noexcept { return m_render_pass_collection; }
	Swapchain *swapchain() const noexcept { return m_swapchain; }
	FramebufferCollection *framebufferCollection() const noexcept { return m_framebuffer_collection; }
	PipelineCollection *pipelineCollection() const noexcept { return m_pipeline_collection; }

	MainLoop *mainLoop() const noexcept { return m_main_loop; }
	AlgoDebugOctree *algoDebugOctree() const noexcept { return m_algo_debug_octree; }

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
private:
	State m_state = State::NotStarted;

	Instance *m_instance = nullptr;
	PhysicalDevice *m_physical_device = nullptr;
	Device *m_device = nullptr;
	DeviceAllocator *m_device_allocator = nullptr;
	TransferManager *m_transfer_manager = nullptr;
	ShaderModuleCollection *m_shader_module_collection = nullptr;
	PipelineCache *m_pipeline_cache = nullptr;
	PipelineLayoutCollection *m_pipeline_layout_collection = nullptr;
	Surface *m_surface = nullptr;
	RenderPassCollection *m_render_pass_collection = nullptr;
	Swapchain *m_swapchain = nullptr;
	FramebufferCollection *m_framebuffer_collection = nullptr;
	PipelineCollection *m_pipeline_collection = nullptr;

	MainLoop *m_main_loop = nullptr;
	AlgoDebugOctree *m_algo_debug_octree = nullptr;

	static Backend s_instance;

	static std::string_view stateToString(State state) noexcept;

	bool loadPreInstanceApi() noexcept;

	enum class StartStopMode {
		Everything,
		SurfaceDependentOnly,
		SwapchainDependentOnly
	};

	bool doStart(Window &window, StartStopMode mode) noexcept;
	void doStop(StartStopMode mode) noexcept;

	Backend() = default;
	Backend(Backend &&) = delete;
	Backend(const Backend &) = delete;
	Backend &operator = (Backend &&) = delete;
	Backend &operator = (const Backend &) = delete;
	~Backend() noexcept;
};

}
