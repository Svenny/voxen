#include <voxen/client/vulkan/physical_device.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/instance.hpp>

#include <voxen/util/assert.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

#include <extras/dyn_array.hpp>

#include <GLFW/glfw3.h>

namespace voxen::client::vulkan
{

PhysicalDevice::PhysicalDevice()
{
	Log::debug("Creating PhysicalDevice");

	auto &backend = VulkanBackend::backend();
	VkInstance instance = *backend.instance();

	uint32_t num_devices = 0;
	VkResult result = backend.vkEnumeratePhysicalDevices(instance, &num_devices, nullptr);
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkEnumeratePhysicalDevices");
	if (num_devices == 0) {
		Log::error("No Vulkan physical devices are present in the system");
		throw MessageException("no Vulkan devices found");
	}

	extras::dyn_array<VkPhysicalDevice> devices(num_devices);
	result = backend.vkEnumeratePhysicalDevices(instance, &num_devices, devices.data());
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkEnumeratePhysicalDevices");

	// TODO: check config first to remember the last selected GPU
	for (const auto &dev : devices) {
		VkPhysicalDeviceProperties props;
		backend.vkGetPhysicalDeviceProperties(dev, &props);

		if (isDeviceSuitable(dev, props)) {
			m_device = dev;
			Log::info("Selected GPU is '{}'", props.deviceName);
			uint32_t api = props.apiVersion;
			Log::info("It supports Vulkan {}.{}.{}", VK_VERSION_MAJOR(api), VK_VERSION_MINOR(api), VK_VERSION_PATCH(api));
			break;
		}
	}

	if (m_device == VK_NULL_HANDLE) {
		Log::error("No suitable Vulkan physical device found in the system");
		throw MessageException("not suitable Vulkan devices found");
	}

	Log::debug("PhysicalDevice created successfully");
}

void PhysicalDevice::logDeviceMemoryStats() const
{
	VkPhysicalDeviceMemoryBudgetPropertiesEXT mem_budget = {};
	mem_budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;

	VkPhysicalDeviceMemoryProperties2 mem_props = {};
	mem_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
	mem_props.pNext = &mem_budget;

	auto &backend = VulkanBackend::backend();
	backend.vkGetPhysicalDeviceMemoryProperties2(m_device, &mem_props);

	uint32_t num_heaps = mem_props.memoryProperties.memoryHeapCount;
	for (uint32_t i = 0; i < num_heaps; i++) {
		constexpr double mult = 1.0 / double(1 << 20); // Bytes to megabytes
		double used = double(mem_budget.heapUsage[i]) * mult;
		double total = double(mem_budget.heapBudget[i]) * mult;
		Log::debug("Using {:.1f}/{:.1f} MB from heap #{}", used, total, i);
	}
}

bool PhysicalDevice::isDeviceSuitable(VkPhysicalDevice device, const VkPhysicalDeviceProperties &props)
{
	Log::debug("Trying GPU '{}'", props.deviceName);

	auto api_version = std::make_pair(VK_VERSION_MAJOR(props.apiVersion),
	                                  VK_VERSION_MINOR(props.apiVersion));
	auto required_api_version = std::make_pair(VulkanInstance::kMinVulkanVersionMajor,
	                                           VulkanInstance::kMinVulkanVersionMinor);
	if (api_version < required_api_version) {
		Log::debug("'{}' is skipped because its supported API {}.{} is lower than required {}.{}",
		           api_version.first, api_version.second, required_api_version.first, required_api_version.second);
		return false;
	}

	// TODO: remove it when saving the selected GPU is available
	if constexpr (BuildConfig::kUseIntegratedGpu) {
		if (props.deviceType != VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
			Log::debug("'{}' is skipped because it's not an integrated GPU", props.deviceName);
			return false;
		}
	} else {
		if (props.deviceType != VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			Log::debug("'{}' is skipped because it's not a discrete GPU", props.deviceName);
			return false;
		}
	}

	if (!populateQueueFamilies(device)) {
		Log::debug("'{}' is skipped because it lacks some queue family capabilities");
		return false;
	}

	return true;
}

bool PhysicalDevice::populateQueueFamilies(VkPhysicalDevice device)
{
	// This shouldn't be called in already constructed object
	vxAssert(m_device == VK_NULL_HANDLE);

	auto &backend = VulkanBackend::backend();

	uint32_t num_families = 0;
	backend.vkGetPhysicalDeviceQueueFamilyProperties(device, &num_families, nullptr);
	extras::dyn_array<VkQueueFamilyProperties> families(num_families);
	backend.vkGetPhysicalDeviceQueueFamilyProperties(device, &num_families, families.data());

	uint32_t graphics_score = UINT32_MAX;
	uint32_t compute_score = UINT32_MAX;
	uint32_t transfer_score = UINT32_MAX;

	for (uint32_t i = 0; i < num_families; i++) {
		// Here we use "queue specialization score" heuristic. We assume that the lesser
		// things a queue family can do, the more efficient it is with these things.
		// At first time we'll pick any queue family with the desired capability.
		// Then we change our choice only if a new one is more specialized.

		// TODO: replace with std::popcount when it is supported
		uint32_t score = __builtin_popcount(families[i].queueFlags);

		if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			if (score < graphics_score) {
				graphics_score = score;
				m_graphics_queue_family = i;
			}
		}

		if (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
			if (score < compute_score) {
				compute_score = score;
				m_compute_queue_family = i;
			}
		}

		if (families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
			if (score < transfer_score) {
				transfer_score = score;
				m_transfer_queue_family = i;
			}
		}
	}

	if (graphics_score == UINT32_MAX) {
		Log::debug("Graphics queue family not found");
		return false;
	} else {
		Log::debug("Preferred graphics queue family index: {}", m_graphics_queue_family);
	}

	if (compute_score == UINT32_MAX) {
		Log::debug("Compute queue family not found");
		return false;
	} else {
		Log::debug("Preferred compute queue family index: {}", m_compute_queue_family);
	}

	if (transfer_score == UINT32_MAX) {
		Log::debug("Transfer queue family not found");
		return false;
	} else {
		Log::debug("Preferred transfer queue family index: {}", m_transfer_queue_family);
	}

	VkInstance instance = *backend.instance();
	// Check if graphics queue family supports present first
	if (glfwGetPhysicalDevicePresentationSupport(instance, device, m_graphics_queue_family)) {
		Log::debug("Graphics queue family can also present");
		m_present_queue_family = m_graphics_queue_family;
		return true;
	}

	Log::debug("Graphics queue family can't present, searching for a separate present queue family");
	// Find any present-capable queue
	for (uint32_t i = 0; i < num_families; i++) {
		if (glfwGetPhysicalDevicePresentationSupport(instance, device, i)) {
			m_present_queue_family = i;
			Log::debug("Preferred present queue family index: {}", m_present_queue_family);
			return true;
		}
	}

	Log::debug("Present queue family not found");
	return false;
}

}
