#include <voxen/client/vulkan/physical_device.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/capabilities.hpp>
#include <voxen/client/vulkan/instance.hpp>
#include <voxen/util/error_condition.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

#include <extras/dyn_array.hpp>

#include <GLFW/glfw3.h>

#include <bit>

namespace voxen::client::vulkan
{

PhysicalDevice::PhysicalDevice()
{
	Log::debug("Creating PhysicalDevice");

	auto &backend = Backend::backend();
	VkInstance instance = backend.instance();

	uint32_t num_devices = 0;
	VkResult result = backend.vkEnumeratePhysicalDevices(instance, &num_devices, nullptr);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkEnumeratePhysicalDevices");
	}

	if (num_devices == 0) {
		Log::error("No Vulkan physical devices are present in the system");
		throw Exception::fromError(VoxenErrc::GfxCapabilityMissing, "no Vulkan devices found");
	}

	extras::dyn_array<VkPhysicalDevice> devices(num_devices);
	result = backend.vkEnumeratePhysicalDevices(instance, &num_devices, devices.data());
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkEnumeratePhysicalDevices");
	}

	VkPhysicalDevice best_device = VK_NULL_HANDLE;
	VkPhysicalDeviceProperties best_props;
	uint32_t best_score = 0;

	// TODO: check config first to remember the last selected GPU
	for (const auto &dev : devices) {
		VkPhysicalDeviceProperties props;
		backend.vkGetPhysicalDeviceProperties(dev, &props);

		if (!isDeviceSuitable(dev, props)) {
			continue;
		}

		uint32_t score = calcDeviceScore(dev, props);

		if (best_device == VK_NULL_HANDLE || score > best_score) {
			best_device = dev;
			best_props = props;
			best_score = score;
		}
	}

	if (best_device == VK_NULL_HANDLE) {
		Log::error("No suitable Vulkan physical device found in the system");
		throw Exception::fromError(VoxenErrc::GfxCapabilityMissing, "no suitable Vulkan devices found");
	}

	// Fill capabilities information with the correct device
	[[maybe_unused]] bool still_suitable = backend.capabilities().selectPhysicalDevice(best_device);
	assert(still_suitable);

	m_device = best_device;
	Log::info("Selected GPU is '{}'", best_props.deviceName);
	uint32_t api = best_props.apiVersion;
	Log::info("It supports Vulkan {}.{}.{}", VK_VERSION_MAJOR(api), VK_VERSION_MINOR(api), VK_VERSION_PATCH(api));

	[[maybe_unused]] bool populated = populateQueueFamilies(m_device);
	// Queues support must be already checked by above per-device loop
	assert(populated);

	Log::debug("PhysicalDevice created successfully");
}

void PhysicalDevice::logDeviceMemoryStats() const
{
	VkPhysicalDeviceMemoryBudgetPropertiesEXT mem_budget = {};
	mem_budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;

	VkPhysicalDeviceMemoryProperties2 mem_props = {};
	mem_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
	mem_props.pNext = &mem_budget;

	auto &backend = Backend::backend();
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

	uint32_t req_version = Capabilities::MIN_VULKAN_VERSION;
	if (props.apiVersion < req_version) {
		Log::debug("'{}' is skipped because its supported API {}.{} is lower than required {}.{}", props.deviceName,
			VK_VERSION_MAJOR(props.apiVersion), VK_VERSION_MINOR(props.apiVersion), VK_VERSION_MAJOR(req_version),
			VK_VERSION_MINOR(req_version));
		return false;
	}

	if (!Backend::backend().capabilities().selectPhysicalDevice(device)) {
		Log::debug("'{}' is skipped because it doesn't pass minimal system requirements", props.deviceName);
		return false;
	}

	if (!populateQueueFamilies(device)) {
		Log::debug("'{}' is skipped because it lacks some queue family capabilities");
		return false;
	}

	return true;
}

bool PhysicalDevice::populateQueueFamilies(VkPhysicalDevice device)
{
	// This can be called several times when trying different devices, reset queus each time
	m_graphics_queue_family = UINT32_MAX;
	m_compute_queue_family = UINT32_MAX;
	m_transfer_queue_family = UINT32_MAX;
	m_present_queue_family = UINT32_MAX;

	auto &backend = Backend::backend();

	uint32_t num_families = 0;
	backend.vkGetPhysicalDeviceQueueFamilyProperties(device, &num_families, nullptr);
	extras::dyn_array<VkQueueFamilyProperties> families(num_families);
	backend.vkGetPhysicalDeviceQueueFamilyProperties(device, &num_families, families.data());

	int graphics_score = INT_MAX;
	int compute_score = INT_MAX;
	int transfer_score = INT_MAX;

	for (uint32_t i = 0; i < num_families; i++) {
		// Here we use "queue specialization score" heuristic. We assume that the lesser
		// things a queue family can do, the more efficient it is with these things.
		// At first time we'll pick any queue family with the desired capability.
		// Then we change our choice only if a new one is more specialized.

		int score = std::popcount(families[i].queueFlags);

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

	if (graphics_score == INT_MAX) {
		Log::debug("Graphics queue family not found");
		return false;
	} else {
		Log::debug("Preferred graphics queue family index: {}", m_graphics_queue_family);
	}

	if (compute_score == INT_MAX) {
		Log::debug("Compute queue family not found");
		return false;
	} else {
		Log::debug("Preferred compute queue family index: {}", m_compute_queue_family);
	}

	if (transfer_score == INT_MAX) {
		Log::debug("Transfer queue family not found");
		return false;
	} else {
		Log::debug("Preferred transfer queue family index: {}", m_transfer_queue_family);
	}

	VkInstance instance = backend.instance();
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

uint32_t PhysicalDevice::calcDeviceScore(VkPhysicalDevice /*device*/, const VkPhysicalDeviceProperties &props)
{
	// This should be enough for most basic setups
	switch (props.deviceType) {
	case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
		return 1;
	case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
		return 2;
	default:
		return 0;
	}
}

} // namespace voxen::client::vulkan
