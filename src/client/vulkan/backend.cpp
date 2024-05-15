#include <voxen/client/vulkan/backend.hpp>

#include <voxen/client/vulkan/algo/terrain_renderer.hpp>
#include <voxen/client/vulkan/capabilities.hpp>
#include <voxen/client/vulkan/descriptor_manager.hpp>
#include <voxen/client/vulkan/descriptor_set_layout.hpp>
#include <voxen/client/vulkan/device.hpp>
#include <voxen/client/vulkan/framebuffer.hpp>
#include <voxen/client/vulkan/high/main_loop.hpp>
#include <voxen/client/vulkan/high/terrain_synchronizer.hpp>
#include <voxen/client/vulkan/high/transfer_manager.hpp>
#include <voxen/client/vulkan/memory.hpp>
#include <voxen/client/vulkan/physical_device.hpp>
#include <voxen/client/vulkan/pipeline.hpp>
#include <voxen/client/vulkan/pipeline_cache.hpp>
#include <voxen/client/vulkan/pipeline_layout.hpp>
#include <voxen/client/vulkan/shader_module.hpp>
#include <voxen/client/vulkan/surface.hpp>
#include <voxen/client/vulkan/swapchain.hpp>
#include <voxen/gfx/vk/vk_instance.hpp>
#include <voxen/util/log.hpp>

#include <GLFW/glfw3.h>

#include <cassert>
#include <new>
#include <tuple>

namespace voxen::client::vulkan
{

struct Backend::Impl {
	template<typename T>
	struct Storage {
		std::aligned_storage_t<sizeof(T), alignof(T)> storage;
	};

	std::tuple<Storage<Capabilities>, Storage<gfx::vk::Instance>, Storage<PhysicalDevice>, Storage<Device>,
		Storage<DeviceAllocator>, Storage<TransferManager>, Storage<ShaderModuleCollection>, Storage<PipelineCache>,
		Storage<DescriptorSetLayoutCollection>, Storage<PipelineLayoutCollection>, Storage<DescriptorManager>,
		Storage<Surface>, Storage<Swapchain>, Storage<FramebufferCollection>, Storage<PipelineCollection>,
		Storage<TerrainSynchronizer>, Storage<MainLoop>, Storage<TerrainRenderer>>
		storage;

	template<typename T, typename... Args>
	void constructModule(T *&ptr, Args &&...args)
	{
		assert(!ptr);
		using S = Storage<std::remove_cvref_t<T>>;
		ptr = new (&std::get<S>(storage)) T(std::forward<Args>(args)...);
	}

	template<typename T>
	void destructModule(T *&ptr) noexcept
	{
		if (ptr) {
			ptr->~T();
			ptr = nullptr;
		}
	}
};

constexpr Backend::Backend(Impl &impl) noexcept : m_impl(impl) {}

static constinit Backend::Impl g_backend_impl;
constinit Backend Backend::s_instance(g_backend_impl);

Backend::~Backend() noexcept
{
	// Backend shouldn't be left in non-stopped state at program termination, should it?
	assert(m_state == State::NotStarted);
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
	if (m_state == State::NotStarted) {
		return;
	}

	Log::info("Stopping Vulkan backend");
	doStop(StartStopMode::Everything);
	Log::info("Vulkan backend stopped");
	m_state = State::NotStarted;
}

bool Backend::drawFrame(const WorldState &state, const GameView &view) noexcept
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
		Log::error("where(): {}:{}", loc.file_name(), loc.line());
	}
	catch (const Exception &e) {
		Log::error("voxen::Exception during rendering a frame");
		Log::error("what(): {}", e.what());
		auto loc = e.where();
		Log::error("where(): {}:{}", loc.file_name(), loc.line());
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
			m_impl.constructModule(m_capabilities);
			m_impl.constructModule(m_instance);

			if (!loadInstanceLevelApi(m_instance->handle())) {
				return false;
			}

			m_impl.constructModule(m_physical_device);
			m_impl.constructModule(m_device);

			m_impl.constructModule(m_device_allocator);
			m_impl.constructModule(m_transfer_manager);
			m_impl.constructModule(m_terrain_synchronizer);

			m_impl.constructModule(m_shader_module_collection);
			m_impl.constructModule(m_pipeline_cache, "pipeline.cache");
			m_impl.constructModule(m_descriptor_set_layout_collection);
			m_impl.constructModule(m_pipeline_layout_collection);
			m_impl.constructModule(m_descriptor_manager);
		}

		if (start_surface_dep) {
			m_impl.constructModule(m_surface, window);
			m_impl.constructModule(m_pipeline_collection);
		}

		if (start_swapchain_dep) {
			m_impl.constructModule(m_swapchain);
			m_impl.constructModule(m_framebuffer_collection);

			m_impl.constructModule(m_main_loop);
			m_impl.constructModule(m_terrain_renderer);
		}

		return true;
	}
	catch (const Exception &e) {
		Log::error("voxen::Exception was catched during starting Vulkan backend");
		Log::error("what(): {}", e.what());
		auto loc = e.where();
		Log::error("where(): {}:{}", loc.file_name(), loc.line());
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
			Log::warn("vkDeviceWaitIdle returned {}, ignoring...", VulkanUtils::getVkResultString(e.result()));
		}
	}

	const bool stop_all = (mode == StartStopMode::Everything);
	const bool stop_surface_dep = (stop_all || mode == StartStopMode::SurfaceDependentOnly);
	const bool stop_swapchain_dep = (stop_surface_dep || mode == StartStopMode::SwapchainDependentOnly);

	if (stop_swapchain_dep) {
		m_impl.destructModule(m_terrain_renderer);
		m_impl.destructModule(m_main_loop);

		m_impl.destructModule(m_framebuffer_collection);
		m_impl.destructModule(m_swapchain);
	}

	if (stop_surface_dep) {
		m_impl.destructModule(m_pipeline_collection);
		m_impl.destructModule(m_surface);
	}

	if (stop_all) {
		m_impl.destructModule(m_descriptor_manager);
		m_impl.destructModule(m_pipeline_layout_collection);
		m_impl.destructModule(m_descriptor_set_layout_collection);
		m_impl.destructModule(m_pipeline_cache);
		m_impl.destructModule(m_shader_module_collection);

		m_impl.destructModule(m_terrain_synchronizer);
		m_impl.destructModule(m_transfer_manager);
		m_impl.destructModule(m_device_allocator);

		m_impl.destructModule(m_device);
		m_impl.destructModule(m_physical_device);
		m_impl.destructModule(m_instance);
		m_impl.destructModule(m_capabilities);
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

} // namespace voxen::client::vulkan
