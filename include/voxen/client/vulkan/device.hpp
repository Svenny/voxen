#pragma once

#include <voxen/client/vulkan/common.hpp>

namespace voxen::client
{

class VulkanDevice {
public:
	VulkanDevice();
	VulkanDevice(VulkanDevice &&) = delete;
	VulkanDevice(const VulkanDevice &) = delete;
	VulkanDevice &operator = (VulkanDevice &&) = delete;
	VulkanDevice &operator = (const VulkanDevice &) = delete;
	~VulkanDevice();

	VkPhysicalDevice physDeviceHandle() const noexcept { return m_phys_device; }
	VkDevice deviceHandle() const noexcept { return m_device; }
private:
	VkPhysicalDevice m_phys_device = VK_NULL_HANDLE;
	VkDevice m_device = VK_NULL_HANDLE;
};

}
