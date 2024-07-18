#include <voxen/client/vulkan/backend.hpp>

#include <voxen/client/vulkan/algo/terrain_renderer.hpp>
#include <voxen/client/vulkan/descriptor_manager.hpp>
#include <voxen/client/vulkan/descriptor_set_layout.hpp>
#include <voxen/client/vulkan/framebuffer.hpp>
#include <voxen/client/vulkan/high/main_loop.hpp>
#include <voxen/client/vulkan/high/terrain_synchronizer.hpp>
#include <voxen/client/vulkan/high/transfer_manager.hpp>
#include <voxen/client/vulkan/memory.hpp>
#include <voxen/client/vulkan/pipeline.hpp>
#include <voxen/client/vulkan/pipeline_cache.hpp>
#include <voxen/client/vulkan/pipeline_layout.hpp>
#include <voxen/client/vulkan/shader_module.hpp>
#include <voxen/gfx/vk/vk_device.hpp>
#include <voxen/gfx/vk/vk_instance.hpp>
#include <voxen/gfx/vk/vk_physical_device.hpp>
#include <voxen/gfx/vk/vk_swapchain.hpp>
#include <voxen/util/log.hpp>

#include <GLFW/glfw3.h>

#include <cassert>
#include <tuple>

namespace voxen::client::vulkan
{

struct Backend::Impl {
	template<typename T>
	struct Storage {
		std::aligned_storage_t<sizeof(T), alignof(T)> storage;
	};

	std::tuple<Storage<gfx::vk::Instance>, Storage<gfx::vk::Device>, Storage<DeviceAllocator>, Storage<TransferManager>,
		Storage<ShaderModuleCollection>, Storage<PipelineCache>, Storage<DescriptorSetLayoutCollection>,
		Storage<PipelineLayoutCollection>, Storage<DescriptorManager>, Storage<gfx::vk::Swapchain>,
		Storage<FramebufferCollection>, Storage<PipelineCollection>, Storage<TerrainSynchronizer>, Storage<MainLoop>,
		Storage<TerrainRenderer>>
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

	if (!doStart(window)) {
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
	doStop();
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
		// Unhandled errors are considered non-recoverable
		m_state = State::Broken;
		Log::error("Vulkan error during rendering a frame");
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

bool Backend::doStart(Window &window) noexcept
{
	try {
		m_impl.constructModule(m_instance);

		if (!loadInstanceLevelApi(m_instance->handle())) {
			return false;
		}

		auto devices = m_instance->enumeratePhysicalDevices();
		auto *device = selectPhysicalDevice(devices);
		if (!device) {
			return false;
		}

		m_impl.constructModule(m_device, *m_instance, *device);

		if (!loadDeviceLevelApi(m_device->handle())) {
			return false;
		}

		m_impl.constructModule(m_device_allocator);
		m_impl.constructModule(m_transfer_manager);
		m_impl.constructModule(m_terrain_synchronizer);

		m_impl.constructModule(m_shader_module_collection);
		m_impl.constructModule(m_pipeline_cache, "pipeline.cache");
		m_impl.constructModule(m_descriptor_set_layout_collection);
		m_impl.constructModule(m_pipeline_layout_collection);
		m_impl.constructModule(m_descriptor_manager);

		m_impl.constructModule(m_swapchain, *m_device, window);
		m_impl.constructModule(m_pipeline_collection);
		m_impl.constructModule(m_framebuffer_collection);

		m_impl.constructModule(m_main_loop);
		m_impl.constructModule(m_terrain_renderer);

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

void Backend::doStop() noexcept
{
	// Finish all outstanding operations on VkDevice, if any. If device is lost, calling
	// vkDeviceWaitIdle will return an error. Ignore it - device is to be destroyed anyway.
	if (m_device) {
		VkResult res = m_device->dt().vkDeviceWaitIdle(m_device->handle());
		if (res != VK_SUCCESS) {
			Log::warn("vkDeviceWaitIdle returned {}, ignoring...", VulkanUtils::getVkResultString(res));
		}
	}

	m_impl.destructModule(m_terrain_renderer);
	m_impl.destructModule(m_main_loop);

	m_impl.destructModule(m_framebuffer_collection);
	m_impl.destructModule(m_pipeline_collection);
	m_impl.destructModule(m_swapchain);

	m_impl.destructModule(m_descriptor_manager);
	m_impl.destructModule(m_pipeline_layout_collection);
	m_impl.destructModule(m_descriptor_set_layout_collection);
	m_impl.destructModule(m_pipeline_cache);
	m_impl.destructModule(m_shader_module_collection);

	m_impl.destructModule(m_terrain_synchronizer);
	m_impl.destructModule(m_transfer_manager);
	m_impl.destructModule(m_device_allocator);

	m_impl.destructModule(m_device);
	unloadDeviceLevelApi();
	m_impl.destructModule(m_instance);
	unloadInstanceLevelApi();
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
	default:
		return "UNKNOWN STATE"sv;
	}
}

gfx::vk::PhysicalDevice *Backend::selectPhysicalDevice(extras::dyn_array<gfx::vk::PhysicalDevice> &devs) noexcept
{
	if (devs.empty()) {
		Log::error("No Vulkan devices available");
		return nullptr;
	}

	// Choose in this order:
	// 1. The first listed discrete GPU
	// 2. If no discrete GPUs, then any integrated one
	// 3. If no integrated GPUs, then just the first listed device
	//
	// Don't do any complex "score" calculations, the user
	// will select GPU manually if he has some unusual setup.
	gfx::vk::PhysicalDevice *discrete = nullptr;
	gfx::vk::PhysicalDevice *integrated = nullptr;
	gfx::vk::PhysicalDevice *other = nullptr;

	Log::debug("Auto-selecting Vulkan device");
	for (auto &dev : devs) {
		std::string_view name = dev.info().props.properties.deviceName;

		if (!gfx::vk::Device::isSupported(dev)) {
			Log::debug("'{}' is does not pass minimal requirements", name);
			continue;
		}

		VkPhysicalDeviceType type = dev.info().props.properties.deviceType;

		if (type == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			Log::debug("'{}' is dGPU, taking it", name);
			discrete = &dev;
			break;
		}

		if (type == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
			Log::debug("'{}' is iGPU, might take it if won't find dGPU", name);
			integrated = &dev;
		} else if (!other) {
			Log::debug("'{}' is neither iGPU nor dGPU, might take it if won't find one", name);
		}
	}

	gfx::vk::PhysicalDevice *chosen = discrete ? discrete : (integrated ? integrated : other);

	if (!chosen) {
		Log::error("No Vulkan devices passing minimal requirements found");
		return nullptr;
	}

	Log::debug("Auto-selected GPU: '{}'", chosen->info().props.properties.deviceName);
	return chosen;
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
