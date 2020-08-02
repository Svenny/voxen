#pragma once

#include <voxen/client/vulkan/common.hpp>
#include <voxen/client/window.hpp>

#include <string_view>

namespace voxen::client::vulkan
{

class Instance;
class PhysicalDevice;
class Device;
class DeviceAllocator;
class TransferManager;
class Surface;
class Swapchain;
class RenderPassCollection;
class FramebufferCollection;
class ShaderModuleCollection;
class PipelineCache;
class PipelineLayoutCollection;
class PipelineCollection;

class MainLoop;
class AlgoDebugOctree;

class Backend {
public:
	enum class State {
		NotStarted,
		Started,
		DeviceLost,
		SurfaceLost,
		SwapchainOutOfDate,
	};

	bool start(Window &window) noexcept;
	void stop() noexcept;

	State state() const noexcept { return m_state; }

	Instance *instance() const noexcept { return m_instance; }
	PhysicalDevice *physicalDevice() const noexcept { return m_physical_device; }
	Device *device() const noexcept { return m_device; }
	DeviceAllocator *deviceAllocator() const noexcept { return m_device_allocator; }
	TransferManager *transferManager() const noexcept { return m_transfer_manager; }
	Surface *surface() const noexcept { return m_surface; }
	Swapchain *swapchain() const noexcept { return m_swapchain; }
	RenderPassCollection *renderPassCollection() const noexcept { return m_render_pass_collection; }
	FramebufferCollection *framebufferCollection() const noexcept { return m_framebuffer_collection; }
	ShaderModuleCollection *shaderModuleCollection() const noexcept { return m_shader_module_collection; }
	PipelineCache *pipelineCache() const noexcept { return m_pipeline_cache; }
	PipelineLayoutCollection *pipelineLayoutCollection() const noexcept { return m_pipeline_layout_collection; }
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
	Surface *m_surface = nullptr;
	Swapchain *m_swapchain = nullptr;
	RenderPassCollection *m_render_pass_collection = nullptr;
	FramebufferCollection *m_framebuffer_collection = nullptr;
	ShaderModuleCollection *m_shader_module_collection = nullptr;
	PipelineCache *m_pipeline_cache = nullptr;
	PipelineLayoutCollection *m_pipeline_layout_collection = nullptr;
	PipelineCollection *m_pipeline_collection = nullptr;

	MainLoop *m_main_loop = nullptr;
	AlgoDebugOctree *m_algo_debug_octree = nullptr;

	static Backend s_instance;

	static std::string_view stateToString(State state) noexcept;

	bool loadPreInstanceApi() noexcept;

	Backend() = default;
	Backend(Backend &&) = delete;
	Backend(const Backend &) = delete;
	Backend &operator = (Backend &&) = delete;
	Backend &operator = (const Backend &) = delete;
	~Backend() noexcept;
};

}
