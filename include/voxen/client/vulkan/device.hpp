#pragma once

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/queue_manager.hpp>

#include <vector>

namespace voxen::client
{

class VulkanDevice {
public:
	explicit VulkanDevice(VulkanBackend &backend);
	VulkanDevice(VulkanDevice &&) = delete;
	VulkanDevice(const VulkanDevice &) = delete;
	VulkanDevice &operator = (VulkanDevice &&) = delete;
	VulkanDevice &operator = (const VulkanDevice &) = delete;
	~VulkanDevice() noexcept;

	void waitIdle();

	VkPhysicalDevice physDeviceHandle() const noexcept { return m_phys_device; }
	VkDevice deviceHandle() const noexcept { return m_device; }
	const VulkanQueueManager &queueManager() const noexcept { return m_queue_manager; }

	operator VkDevice() const noexcept { return m_device; }
private:
	VulkanBackend &m_backend;
	VkPhysicalDevice m_phys_device = VK_NULL_HANDLE;
	VkDevice m_device = VK_NULL_HANDLE;
	VulkanQueueManager m_queue_manager;

	bool pickPhysicalDevice();
	bool isDeviceSuitable(VkPhysicalDevice device);
	std::vector<const char *> getRequiredDeviceExtensions();
	VkPhysicalDeviceFeatures getRequiredFeatures();
	bool createLogicalDevice();
	void destroyDevice() noexcept;
};

}
