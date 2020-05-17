#include <voxen/client/vulkan/backend.hpp>

#include <voxen/client/vulkan/instance.hpp>
#include <voxen/client/vulkan/device.hpp>
#include <voxen/util/log.hpp>

#include <GLFW/glfw3.h>

namespace voxen::client
{

VulkanBackend::~VulkanBackend() noexcept {
	stop();
}

bool VulkanBackend::start() noexcept {
	if (m_state != State::NotStarted) {
		Log::warn("Vulkan backend is already started");
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
		m_instance = new VulkanInstance(*this);
		if (!loadInstanceLevelApi(m_instance->handle())) {
			Log::error("Loading instance-level Vulkan API failed");
			stop();
			return false;
		}

		m_device = new VulkanDevice(*this);
		if (!loadDeviceLevelApi(m_device->deviceHandle())) {
			Log::error("Loading device-level Vulkan API failed");
			stop();
			return false;
		}
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

	delete m_device;
	m_device = nullptr;
	delete m_instance;
	m_instance = nullptr;
	unloadApi();

	Log::info("Vulkan backend stopped");
	m_state = State::NotStarted;
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

bool VulkanBackend::loadDeviceLevelApi(VkDevice device) noexcept {
#define TRY_LOAD(name) \
	name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device, #name)); \
	if (!name) { \
		Log::warn("Can't load device-level Vulkan API {}", #name); \
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

void VulkanBackend::unloadApi() noexcept {
#define VK_INSTANCE_API_ENTRY(name) name = nullptr;
#define VK_DEVICE_API_ENTRY(name) name = nullptr;
#include <voxen/client/vulkan/api_table.in>
#undef VK_DEVICE_API_ENTRY
#undef VK_INSTANCE_API_ENTRY
}

}
