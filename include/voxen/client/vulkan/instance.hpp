#pragma once

#include <voxen/client/vulkan/common.hpp>

namespace voxen::client
{

class VulkanInstance {
public:
	VulkanInstance();
	VulkanInstance(VulkanInstance &&) = delete;
	VulkanInstance(const VulkanInstance &) = delete;
	VulkanInstance &operator = (VulkanInstance &&) = delete;
	VulkanInstance &operator = (const VulkanInstance &) = delete;
	~VulkanInstance();

	VkInstance handle() const noexcept { return m_handle; }

	// Minimal supported Vulkan version
	constexpr static uint32_t kMinVulkanVersionMajor = 1;
	constexpr static uint32_t kMinVulkanVersionMinor = 1;
private:
	VkInstance m_handle = VK_NULL_HANDLE;

	bool checkVulkanSupport() const;
	bool createInstance();
};

}
