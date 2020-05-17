#pragma once

#include <voxen/client/vulkan/backend.hpp>

namespace voxen::client
{

class VulkanDevice {
public:
	VulkanDevice(VulkanBackend &backend);
	VulkanDevice(VulkanDevice &&) = delete;
	VulkanDevice(const VulkanDevice &) = delete;
	VulkanDevice &operator = (VulkanDevice &&) = delete;
	VulkanDevice &operator = (const VulkanDevice &) = delete;
	~VulkanDevice() noexcept;

	VkPhysicalDevice physDeviceHandle() const noexcept { return m_phys_device; }
	VkDevice deviceHandle() const noexcept { return m_device; }
private:
	VulkanBackend &m_backend;
	VkPhysicalDevice m_phys_device = VK_NULL_HANDLE;
	VkDevice m_device = VK_NULL_HANDLE;
};

}
