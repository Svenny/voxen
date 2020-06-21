#pragma once

#include <voxen/client/vulkan/backend.hpp>

#include <vector>

namespace voxen::client
{

class VulkanQueueManager {
public:
	VulkanQueueManager() = default;
	VulkanQueueManager(VulkanQueueManager &&) = delete;
	VulkanQueueManager(const VulkanQueueManager &) = delete;
	VulkanQueueManager &operator = (VulkanQueueManager &&) = delete;
	VulkanQueueManager &operator = (const VulkanQueueManager &) = delete;
	~VulkanQueueManager() = default;

	bool findFamilies(VulkanBackend &backend, VkPhysicalDevice device);
	std::vector<VkDeviceQueueCreateInfo> getCreateInfos() const;
	void getHandles(VulkanBackend &backend, VkDevice device);

	// Queue family with this index has VK_QUEUE_GRAPHICS_BIT and is preferred for graphics workloads
	uint32_t graphicsQueueFamily() const noexcept { return m_graphics_queue_family; }
	// Queue family with this index has VK_QUEUE_COMPUTE_BIT and is preferred for compute workloads
	uint32_t computeQueueFamily() const noexcept { return m_compute_queue_family; }
	// Queue family with this index has VK_QUEUE_TRANSFER_BIT and is preferred for transfer workloads
	uint32_t transferQueueFamily() const noexcept { return m_transfer_queue_family; }
	// Queue family with this index is capable of presenting swapchain surfaces
	uint32_t presentQueueFamily() const noexcept { return m_present_queue_family; }

	VkQueue graphicsQueue() const noexcept { return m_graphics_queue; }
	VkQueue computeQueue() const noexcept { return m_compute_queue; }
	VkQueue transferQueue() const noexcept { return m_transfer_queue; }
	VkQueue presentQueue() const noexcept { return m_present_queue; }
private:
	uint32_t m_graphics_queue_family = UINT32_MAX;
	uint32_t m_compute_queue_family = UINT32_MAX;
	uint32_t m_transfer_queue_family = UINT32_MAX;
	uint32_t m_present_queue_family = UINT32_MAX;

	VkQueue m_graphics_queue = VK_NULL_HANDLE;
	VkQueue m_compute_queue = VK_NULL_HANDLE;
	VkQueue m_transfer_queue = VK_NULL_HANDLE;
	VkQueue m_present_queue = VK_NULL_HANDLE;
};

}
