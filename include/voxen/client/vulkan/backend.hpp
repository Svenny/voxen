#pragma once

#include <voxen/client/vulkan/common.hpp>
#include <voxen/client/window.hpp>

#include <string_view>

namespace voxen::client
{

class VulkanSurface;
class VulkanSwapchain;
class VulkanRenderPassCollection;
class VulkanFramebufferCollection;
class VulkanShaderModuleCollection;
class VulkanPipelineCache;
class VulkanPipelineLayoutCollection;
class VulkanPipelineCollection;

namespace vulkan
{

class Instance;
class PhysicalDevice;
class Device;
class DeviceAllocator;
class TransferManager;
class MainLoop;
class AlgoDebugOctree;

}

class VulkanBackend {
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

	vulkan::Instance *instance() const noexcept { return m_instance; }
	vulkan::PhysicalDevice *physicalDevice() const noexcept { return m_physical_device; }
	vulkan::Device *device() const noexcept { return m_device; }
	vulkan::DeviceAllocator *deviceAllocator() const noexcept { return m_device_allocator; }
	vulkan::TransferManager *transferManager() const noexcept { return m_transfer_manager; }
	VulkanSurface *surface() const noexcept { return m_surface; }
	VulkanSwapchain *swapchain() const noexcept { return m_swapchain; }
	VulkanRenderPassCollection *renderPassCollection() const noexcept { return m_render_pass_collection; }
	VulkanFramebufferCollection *framebufferCollection() const noexcept { return m_framebuffer_collection; }
	VulkanShaderModuleCollection *shaderModuleCollection() const noexcept { return m_shader_module_collection; }
	VulkanPipelineCache *pipelineCache() const noexcept { return m_pipeline_cache; }
	VulkanPipelineLayoutCollection *pipelineLayoutCollection() const noexcept { return m_pipeline_layout_collection; }
	VulkanPipelineCollection *pipelineCollection() const noexcept { return m_pipeline_collection; }
	vulkan::MainLoop *mainLoop() const noexcept { return m_main_loop; }

	vulkan::AlgoDebugOctree *algoDebugOctree() const noexcept { return m_algo_debug_octree; }

	bool loadInstanceLevelApi(VkInstance instance) noexcept;
	void unloadInstanceLevelApi() noexcept;
	bool loadDeviceLevelApi(VkDevice device) noexcept;
	void unloadDeviceLevelApi() noexcept;

	// Declare pointers to Vulkan API entry points, moved
	// into a separate file because of size and ugliness
#include "api_table_declare.in"

	// VulkanBackend is the only Vulkan-related class designed
	// to be a singleton. The main arguments for this decision:
	// 1. Launching multiple backends makes no sense
	// 2. With non-singleton design each downstream entity would have to store reference
	//    to its backend, needlessly (because of p.1) increasing objects' and code size
	// 3. There is a lot of downstream entities (which strengthens p.2)
	static VulkanBackend &backend() noexcept { return s_instance; }
private:
	State m_state = State::NotStarted;

	vulkan::Instance *m_instance = nullptr;
	vulkan::PhysicalDevice *m_physical_device = nullptr;
	vulkan::Device *m_device = nullptr;
	vulkan::DeviceAllocator *m_device_allocator = nullptr;
	vulkan::TransferManager *m_transfer_manager = nullptr;
	VulkanSurface *m_surface = nullptr;
	VulkanSwapchain *m_swapchain = nullptr;
	VulkanRenderPassCollection *m_render_pass_collection = nullptr;
	VulkanFramebufferCollection *m_framebuffer_collection = nullptr;
	VulkanShaderModuleCollection *m_shader_module_collection = nullptr;
	VulkanPipelineCache *m_pipeline_cache = nullptr;
	VulkanPipelineLayoutCollection *m_pipeline_layout_collection = nullptr;
	VulkanPipelineCollection *m_pipeline_collection = nullptr;
	vulkan::MainLoop *m_main_loop = nullptr;

	vulkan::AlgoDebugOctree *m_algo_debug_octree = nullptr;

	static VulkanBackend s_instance;

	static std::string_view stateToString(State state) noexcept;

	bool loadPreInstanceApi() noexcept;

	VulkanBackend() = default;
	VulkanBackend(VulkanBackend &&) = delete;
	VulkanBackend(const VulkanBackend &) = delete;
	VulkanBackend &operator = (VulkanBackend &&) = delete;
	VulkanBackend &operator = (const VulkanBackend &) = delete;
	~VulkanBackend() noexcept;
};

}
