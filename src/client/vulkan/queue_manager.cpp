#include <voxen/client/vulkan/queue_manager.hpp>

#include <voxen/client/vulkan/instance.hpp>
#include <voxen/util/log.hpp>

#include <extras/dyn_array.hpp>

#include <GLFW/glfw3.h>

namespace voxen::client
{

bool VulkanQueueManager::findFamilies(VulkanBackend &backend, VkPhysicalDevice device) {
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

	VkInstance instance = backend.instance()->handle();
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

std::vector<VkDeviceQueueCreateInfo> VulkanQueueManager::getCreateInfos() const {
	// VkDeviceQueueCreateInfo uses a pointer to a float for priorities array.
	// We don't use priorities now, so just point it to a single constant.
	static constexpr float QUEUE_PRIORITY = 1.0f;

	std::vector<VkDeviceQueueCreateInfo> result;

	VkDeviceQueueCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	info.queueCount = 1;
	info.pQueuePriorities = &QUEUE_PRIORITY;

	// Add `info` to `result`, setting queue family index
	// to `idx` and preventing possible duplicates
	auto addItem = [&result, &info](uint32_t idx) {
		info.queueFamilyIndex = idx;
		for (const auto &item : result)
			if (item.queueFamilyIndex == info.queueFamilyIndex)
				return;
		result.push_back(info);
	};

	addItem(m_graphics_queue_family);
	addItem(m_compute_queue_family);
	addItem(m_transfer_queue_family);
	addItem(m_present_queue_family);

	return result;
}

void VulkanQueueManager::getHandles(VulkanBackend &backend, VkDevice device) {
	// HACK: this method is called in `VulkanDevice` constructor just after
	// creating VkDevice. Because control is not yet returned to VulkanBackend,
	// device-level API is not loaded at this point. So here we have to do it ourselves.
	auto fun = reinterpret_cast<PFN_vkGetDeviceQueue>(backend.vkGetDeviceProcAddr(device, "vkGetDeviceQueue"));
	if (!fun) {
		Log::error("Can't load vkGetDeviceQueue entry point");
		throw MessageException("vkGetDeviceProcAddr failed");
	}

	fun(device, m_graphics_queue_family, 0, &m_graphics_queue);
	fun(device, m_compute_queue_family, 0, &m_compute_queue);
	fun(device, m_transfer_queue_family, 0, &m_transfer_queue);
	fun(device, m_present_queue_family, 0, &m_present_queue);
}

}
