#pragma once

#include <voxen/client/vulkan/common.hpp>

#include <memory>

namespace voxen::client
{

class VulkanBase {
public:
	VulkanBase(VulkanBase &&) = delete;
	VulkanBase(const VulkanBase &) = delete;
	VulkanBase &operator = (VulkanBase &&) = delete;
	VulkanBase &operator = (const VulkanBase &) = delete;
	~VulkanBase();

	VkInstance vkInstnace() const noexcept { return m_instance; }

	static VulkanBase *instance() noexcept { return g_instance.get(); }
	// Minimal supported Vulkan version
	constexpr static uint32_t kMinVulkanVersionMajor = 1;
	constexpr static uint32_t kMinVulkanVersionMinor = 1;
private:
	VkInstance m_instance = VK_NULL_HANDLE;

	bool checkVulkanSupport() const;
	bool createInstance();

	friend class Render;
	VulkanBase();
	static std::unique_ptr<VulkanBase> g_instance;
};

}
