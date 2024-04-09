#pragma once

#include <voxen/client/vulkan/common.hpp>

namespace voxen::client::vulkan
{

class PhysicalDevice {
public:
	PhysicalDevice();
	PhysicalDevice(PhysicalDevice &&) = delete;
	PhysicalDevice(const PhysicalDevice &) = delete;
	PhysicalDevice &operator=(PhysicalDevice &&) = delete;
	PhysicalDevice &operator=(const PhysicalDevice &) = delete;
	~PhysicalDevice() = default;

	void logDeviceMemoryStats() const;

	// Queue family with this index has VK_QUEUE_GRAPHICS_BIT and is preferred for graphics workloads
	uint32_t graphicsQueueFamily() const noexcept { return m_graphics_queue_family; }
	// Queue family with this index has VK_QUEUE_COMPUTE_BIT and is preferred for compute workloads
	uint32_t computeQueueFamily() const noexcept { return m_compute_queue_family; }
	// Queue family with this index has VK_QUEUE_TRANSFER_BIT and is preferred for transfer workloads
	uint32_t transferQueueFamily() const noexcept { return m_transfer_queue_family; }
	// Queue family with this index is capable of presenting swapchain surfaces
	uint32_t presentQueueFamily() const noexcept { return m_present_queue_family; }

	operator VkPhysicalDevice() const noexcept { return m_device; }

private:
	VkPhysicalDevice m_device = VK_NULL_HANDLE;
	uint32_t m_graphics_queue_family = UINT32_MAX;
	uint32_t m_compute_queue_family = UINT32_MAX;
	uint32_t m_transfer_queue_family = UINT32_MAX;
	uint32_t m_present_queue_family = UINT32_MAX;

	bool isDeviceSuitable(VkPhysicalDevice device, const VkPhysicalDeviceProperties &props);
	bool populateQueueFamilies(VkPhysicalDevice device);
	// Calculate score to select the "best" GPU if several are available (bigger is better)
	static uint32_t calcDeviceScore(VkPhysicalDevice device, const VkPhysicalDeviceProperties &props);
};

} // namespace voxen::client::vulkan
