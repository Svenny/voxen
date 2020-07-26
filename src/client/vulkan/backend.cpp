#include <voxen/client/vulkan/backend.hpp>

#include <voxen/client/vulkan/instance.hpp>
#include <voxen/client/vulkan/device.hpp>
#include <voxen/client/vulkan/framebuffer.hpp>
#include <voxen/client/vulkan/main_loop.hpp>
#include <voxen/client/vulkan/memory.hpp>
#include <voxen/client/vulkan/physical_device.hpp>
#include <voxen/client/vulkan/pipeline.hpp>
#include <voxen/client/vulkan/pipeline_cache.hpp>
#include <voxen/client/vulkan/pipeline_layout.hpp>
#include <voxen/client/vulkan/render_pass.hpp>
#include <voxen/client/vulkan/shader_module.hpp>
#include <voxen/client/vulkan/surface.hpp>
#include <voxen/client/vulkan/swapchain.hpp>

#include <voxen/util/assert.hpp>
#include <voxen/util/log.hpp>

#include <GLFW/glfw3.h>

namespace voxen::client
{

VulkanBackend VulkanBackend::s_instance;

VulkanBackend::~VulkanBackend() noexcept {
	// Backend shouldn't be left in non-stopped state at program termination, should it?
	vxAssert(m_state == State::NotStarted);
	// But if assertions are disabled...
	if (m_state != State::NotStarted) {
		Log::warn("VulkanBackend left in non-stopped state [{}]!", stateToString(m_state));
		stop();
	}
}

bool VulkanBackend::start(Window &window) noexcept {
	if (m_state != State::NotStarted) {
		Log::warn("Cannot start Vulkan backend - it's in state [{}] now", stateToString(m_state));
		return true;
	}

	// Set this state early so calling `stop()` on failure will actually do something
	m_state = State::Started;
	Log::info("Starting Vulkan backend");

	if (!loadPreInstanceApi()) {
		Log::error("Loading pre-instance Vulkan API failed");
		stop();
		return false;
	}

	try {
		m_instance = new vulkan::Instance;
		m_physical_device = new vulkan::PhysicalDevice;
		m_device = new vulkan::Device;
		m_device_allocator = new vulkan::DeviceAllocator;
		m_surface = new VulkanSurface(window);
		m_swapchain = new VulkanSwapchain;
		m_render_pass_collection = new VulkanRenderPassCollection;
		m_framebuffer_collection = new VulkanFramebufferCollection;
		m_shader_module_collection = new VulkanShaderModuleCollection;
		m_pipeline_cache = new VulkanPipelineCache("pipeline.cache");
		m_pipeline_layout_collection = new VulkanPipelineLayoutCollection;
		m_pipeline_collection = new VulkanPipelineCollection;
		m_main_loop = new VulkanMainLoop;
	}
	catch (const Exception &e) {
		Log::error("voxen::Exception was catched during starting Vulkan backend");
		Log::error("what(): {}", e.what());
		auto loc = e.where();
		Log::error("where(): {}:{} ({})", loc.file_name(), loc.line(), loc.function_name());
		stop();
		return false;
	}
	catch (const std::exception &e) {
		Log::error("std::exception was catched during starting Vulkan backend");
		Log::error("what(): {}", e.what());
		stop();
		return false;
	}
	catch (...) {
		Log::error("An unknown exception was catched during starting Vulkan backend");
		stop();
		return false;
	}

	Log::info("Vulkan backend started");
	return true;
}

void VulkanBackend::stop() noexcept {
	if (m_state == State::NotStarted)
		return;

	Log::info("Stopping Vulkan backend");

	// Finish all outstanding operations on VkDevice, if any. If device is lost, calling
	// vkDeviceWaitIdle will return an error. Ignore it - device is to be destroyed anyway.
	if (m_device) {
		try {
			m_device->waitIdle();
		}
		catch (const VulkanException &e) {
			Log::warn("vkDeviceWaitIdle returned {}, ignoring...", getVkResultString(e.result()));
		}
	}

	delete m_main_loop;
	m_main_loop = nullptr;
	delete m_pipeline_collection;
	m_pipeline_collection = nullptr;
	delete m_pipeline_layout_collection;
	m_pipeline_layout_collection = nullptr;
	delete m_pipeline_cache;
	m_pipeline_cache = nullptr;
	delete m_shader_module_collection;
	m_shader_module_collection = nullptr;
	delete m_framebuffer_collection;
	m_framebuffer_collection = nullptr;
	delete m_render_pass_collection;
	m_render_pass_collection = nullptr;
	delete m_swapchain;
	m_swapchain = nullptr;
	delete m_surface;
	m_surface = nullptr;
	delete m_device_allocator;
	m_device_allocator = nullptr;
	delete m_device;
	m_device = nullptr;
	delete m_physical_device;
	m_physical_device = nullptr;
	delete m_instance;
	m_instance = nullptr;

	Log::info("Vulkan backend stopped");
	m_state = State::NotStarted;
}

std::string_view VulkanBackend::stateToString(State state) noexcept {
	using namespace std::literals;
	switch (state) {
	case State::NotStarted:
		return "Not started"sv;
	case State::Started:
		return "Started"sv;
	case State::DeviceLost:
		return "Device lost"sv;
	case State::SurfaceLost:
		return "Surface lost"sv;
	case State::SwapchainOutOfDate:
		return "Swapchain out of date"sv;
	default:
		return "UNKNOWN STATE"sv;
	}
}

bool VulkanBackend::loadPreInstanceApi() noexcept {
#define TRY_LOAD(name) \
	name = reinterpret_cast<PFN_##name>(glfwGetInstanceProcAddress(VK_NULL_HANDLE, #name)); \
	if (!name) { \
		Log::warn("Can't load pre-instance Vulkan API {}", #name); \
		is_ok = false; \
	}

	bool is_ok = true;
	TRY_LOAD(vkEnumerateInstanceVersion)
	TRY_LOAD(vkEnumerateInstanceExtensionProperties)
	TRY_LOAD(vkEnumerateInstanceLayerProperties)
	TRY_LOAD(vkCreateInstance)

	return is_ok;
#undef TRY_LOAD
}

bool VulkanBackend::loadInstanceLevelApi(VkInstance instance) noexcept {
#define TRY_LOAD(name) \
	name = reinterpret_cast<PFN_##name>(glfwGetInstanceProcAddress(instance, #name)); \
	if (!name) { \
		Log::warn("Can't load instance-level Vulkan API {}", #name); \
		is_ok = false; \
	}

	bool is_ok = true;
#define VK_INSTANCE_API_ENTRY(name) TRY_LOAD(name)
#define VK_DEVICE_API_ENTRY(name)
#include <voxen/client/vulkan/api_table.in>
#undef VK_DEVICE_API_ENTRY
#undef VK_INSTANCE_API_ENTRY

	return is_ok;
#undef TRY_LOAD
}

void VulkanBackend::unloadInstanceLevelApi() noexcept {
#define VK_INSTANCE_API_ENTRY(name) name = nullptr;
#define VK_DEVICE_API_ENTRY(name)
#include <voxen/client/vulkan/api_table.in>
#undef VK_DEVICE_API_ENTRY
#undef VK_INSTANCE_API_ENTRY
}

bool VulkanBackend::loadDeviceLevelApi(VkDevice device) noexcept {
#define TRY_LOAD(name) \
	name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device, #name)); \
	if (!name) { \
		Log::warn("Can't load device-level Vulkan API {}", #name); \
		is_ok = false; \
	}

	bool is_ok = true;
#define VK_INSTANCE_API_ENTRY(name)
#define VK_DEVICE_API_ENTRY(name) TRY_LOAD(name)
#include <voxen/client/vulkan/api_table.in>
#undef VK_DEVICE_API_ENTRY
#undef VK_INSTANCE_API_ENTRY

	return is_ok;
#undef TRY_LOAD
}

void VulkanBackend::unloadDeviceLevelApi() noexcept {
#define VK_INSTANCE_API_ENTRY(name)
#define VK_DEVICE_API_ENTRY(name) name = nullptr;
#include <voxen/client/vulkan/api_table.in>
#undef VK_DEVICE_API_ENTRY
#undef VK_INSTANCE_API_ENTRY
}

}
