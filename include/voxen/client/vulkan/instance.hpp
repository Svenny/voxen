#pragma once

#include <voxen/client/vulkan/backend.hpp>

namespace voxen::client
{

class VulkanInstance {
public:
	// Minimal supported Vulkan version
	constexpr static uint32_t kMinVulkanVersionMajor = 1;
	constexpr static uint32_t kMinVulkanVersionMinor = 1;

	explicit VulkanInstance(VulkanBackend &backend);
	VulkanInstance(VulkanInstance &&) = delete;
	VulkanInstance(const VulkanInstance &) = delete;
	VulkanInstance &operator = (VulkanInstance &&) = delete;
	VulkanInstance &operator = (const VulkanInstance &) = delete;
	~VulkanInstance() noexcept;

	VkInstance handle() const noexcept { return m_handle; }
private:
	VulkanBackend &m_backend;
	VkInstance m_handle = VK_NULL_HANDLE;

	bool checkVulkanSupport() const;
	bool createInstance();
};

}
