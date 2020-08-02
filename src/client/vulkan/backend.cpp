#include <voxen/client/vulkan/backend.hpp>

#include <voxen/client/vulkan/high/main_loop.hpp>
#include <voxen/client/vulkan/high/transfer_manager.hpp>
#include <voxen/client/vulkan/device.hpp>
#include <voxen/client/vulkan/framebuffer.hpp>
#include <voxen/client/vulkan/instance.hpp>
#include <voxen/client/vulkan/memory.hpp>
#include <voxen/client/vulkan/physical_device.hpp>
#include <voxen/client/vulkan/pipeline.hpp>
#include <voxen/client/vulkan/pipeline_cache.hpp>
#include <voxen/client/vulkan/pipeline_layout.hpp>
#include <voxen/client/vulkan/render_pass.hpp>
#include <voxen/client/vulkan/shader_module.hpp>
#include <voxen/client/vulkan/surface.hpp>
#include <voxen/client/vulkan/swapchain.hpp>

#include <voxen/client/vulkan/algo/debug_octree.hpp>

#include <voxen/util/assert.hpp>
#include <voxen/util/log.hpp>

#include <GLFW/glfw3.h>

namespace voxen::client::vulkan
{

Backend Backend::s_instance;

Backend::~Backend() noexcept
{
	// Backend shouldn't be left in non-stopped state at program termination, should it?
	vxAssert(m_state == State::NotStarted);
	// But if assertions are disabled...
	if (m_state != State::NotStarted) {
		Log::warn("Backend left in non-stopped state [{}]!", stateToString(m_state));
		stop();
	}
}

bool Backend::start(Window &window) noexcept
{
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

	if (!doStart(window, StartStopMode::Everything)) {
		stop();
		return false;
	}

	Log::info("Vulkan backend started");
	return true;
}

void Backend::stop() noexcept
{
	if (m_state == State::NotStarted)
		return;

	Log::info("Stopping Vulkan backend");
	doStop(StartStopMode::Everything);
	Log::info("Vulkan backend stopped");
	m_state = State::NotStarted;
}

bool Backend::drawFrame(const World &state, const GameView &view) noexcept
{
	if (!m_main_loop) {
		Log::error("No MainLoop - refusing to draw the frame");
		return false;
	}

	try {
		m_main_loop->drawFrame(state, view);
		return true;
	}
	catch (const VulkanException &e) {
		if (e.result() == VK_ERROR_SURFACE_LOST_KHR) {
			m_state = State::SurfaceLost;
			Log::error("Surface lost during rendering a frame");
		} else if (e.result() == VK_ERROR_OUT_OF_DATE_KHR) {
			m_state = State::SwapchainOutOfDate;
			Log::error("Swapchain went out of date during rendering a frame");
		} else {
			// Other errors are considered non-recoverable
			m_state = State::Broken;
			Log::error("Vulkan error during rendering a frame");
		}
		Log::error("what(): {}", e.what());
		auto loc = e.where();
		Log::error("where(): {}:{} ({})", loc.file_name(), loc.line(), loc.function_name());
	}
	catch (const Exception &e) {
		Log::error("voxen::Exception during rendering a frame");
		Log::error("what(): {}", e.what());
		auto loc = e.where();
		Log::error("where(): {}:{} ({})", loc.file_name(), loc.line(), loc.function_name());
	}
	catch (const std::exception &e) {
		Log::error("std::exception during rendering a frame");
		Log::error("what(): {}", e.what());
	}
	catch (...) {
		Log::error("An unknown exception during rendering a frame");
	}

	return false;
}

bool Backend::recreateSurface(Window &window) noexcept
{
	if (m_state != State::SurfaceLost) {
		Log::warn("recreateSurface() called when surface is not lost");
		return true;
	}

	doStop(StartStopMode::SurfaceDependentOnly);

	if (doStart(window, StartStopMode::SurfaceDependentOnly)) {
		m_state = State::Started;
		return true;
	}
	return false;
}

bool Backend::recreateSwapchain(Window &window) noexcept
{
	if (m_state != State::SwapchainOutOfDate) {
		Log::warn("recreateSwapchain() called when swapchain is not out of date");
		return true;
	}

	doStop(StartStopMode::SwapchainDependentOnly);

	if (doStart(window, StartStopMode::SwapchainDependentOnly)) {
		m_state = State::Started;
		return true;
	}
	return false;
}

bool Backend::doStart(Window &window, StartStopMode mode) noexcept
{
	const bool start_all = (mode == StartStopMode::Everything);
	const bool start_surface_dep = (start_all || mode == StartStopMode::SurfaceDependentOnly);
	const bool start_swapchain_dep = (start_surface_dep || mode == StartStopMode::SwapchainDependentOnly);

	try {
		if (start_all) {
			m_instance = new Instance;
			m_physical_device = new PhysicalDevice;
			m_device = new Device;
			m_device_allocator = new DeviceAllocator;
			m_transfer_manager = new TransferManager;
			m_shader_module_collection = new ShaderModuleCollection;
			m_pipeline_cache = new PipelineCache("pipeline.cache");
			m_pipeline_layout_collection = new PipelineLayoutCollection;
		}

		if (start_surface_dep) {
			m_surface = new Surface(window);
			m_render_pass_collection = new RenderPassCollection;
		}

		if (start_swapchain_dep) {
			m_swapchain = new Swapchain;
			m_framebuffer_collection = new FramebufferCollection;
			m_pipeline_collection = new PipelineCollection;

			m_main_loop = new MainLoop;
			m_algo_debug_octree = new AlgoDebugOctree;
		}

		return true;
	}
	catch (const Exception &e) {
		Log::error("voxen::Exception was catched during starting Vulkan backend");
		Log::error("what(): {}", e.what());
		auto loc = e.where();
		Log::error("where(): {}:{} ({})", loc.file_name(), loc.line(), loc.function_name());
	}
	catch (const std::exception &e) {
		Log::error("std::exception was catched during starting Vulkan backend");
		Log::error("what(): {}", e.what());
	}
	catch (...) {
		Log::error("An unknown exception was catched during starting Vulkan backend");
	}

	return false;
}

void Backend::doStop(StartStopMode mode) noexcept
{
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

	const bool stop_all = (mode == StartStopMode::Everything);
	const bool stop_surface_dep = (stop_all || mode == StartStopMode::SurfaceDependentOnly);
	const bool stop_swapchain_dep = (stop_surface_dep || mode == StartStopMode::SwapchainDependentOnly);

	if (stop_swapchain_dep) {
		delete m_algo_debug_octree;
		m_algo_debug_octree = nullptr;
		delete m_main_loop;
		m_main_loop = nullptr;

		delete m_pipeline_collection;
		m_pipeline_collection = nullptr;
		delete m_framebuffer_collection;
		m_framebuffer_collection = nullptr;
		delete m_swapchain;
		m_swapchain = nullptr;
	}

	if (stop_surface_dep) {
		delete m_render_pass_collection;
		m_render_pass_collection = nullptr;
		delete m_surface;
		m_surface = nullptr;
	}

	if (stop_all) {
		delete m_pipeline_layout_collection;
		m_pipeline_layout_collection = nullptr;
		delete m_pipeline_cache;
		m_pipeline_cache = nullptr;
		delete m_shader_module_collection;
		m_shader_module_collection = nullptr;
		delete m_transfer_manager;
		m_transfer_manager = nullptr;
		delete m_device_allocator;
		m_device_allocator = nullptr;
		delete m_device;
		m_device = nullptr;
		delete m_physical_device;
		m_physical_device = nullptr;
		delete m_instance;
		m_instance = nullptr;
	}
}

std::string_view Backend::stateToString(State state) noexcept
{
	using namespace std::literals;
	switch (state) {
	case State::NotStarted:
		return "Not started"sv;
	case State::Started:
		return "Started"sv;
	case State::Broken:
		return "Broken"sv;
	case State::SurfaceLost:
		return "Surface lost"sv;
	case State::SwapchainOutOfDate:
		return "Swapchain out of date"sv;
	default:
		return "UNKNOWN STATE"sv;
	}
}

// API lodaing/unloading methods

bool Backend::loadPreInstanceApi() noexcept
{
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

bool Backend::loadInstanceLevelApi(VkInstance instance) noexcept
{
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

void Backend::unloadInstanceLevelApi() noexcept
{
#define VK_INSTANCE_API_ENTRY(name) name = nullptr;
#define VK_DEVICE_API_ENTRY(name)
#include <voxen/client/vulkan/api_table.in>
#undef VK_DEVICE_API_ENTRY
#undef VK_INSTANCE_API_ENTRY
}

bool Backend::loadDeviceLevelApi(VkDevice device) noexcept
{
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

void Backend::unloadDeviceLevelApi() noexcept
{
#define VK_INSTANCE_API_ENTRY(name)
#define VK_DEVICE_API_ENTRY(name) name = nullptr;
#include <voxen/client/vulkan/api_table.in>
#undef VK_DEVICE_API_ENTRY
#undef VK_INSTANCE_API_ENTRY
}

}
