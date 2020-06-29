#pragma once

#include <voxen/client/vulkan/common.hpp>

namespace voxen::client
{

class VulkanShaderModule {
public:
	explicit VulkanShaderModule(const char *path);
	VulkanShaderModule(VulkanShaderModule &&) = delete;
	VulkanShaderModule(const VulkanShaderModule &) = delete;
	VulkanShaderModule &operator = (VulkanShaderModule &&) = delete;
	VulkanShaderModule &operator = (const VulkanShaderModule &) = delete;
	~VulkanShaderModule() noexcept;

	operator VkShaderModule() const noexcept { return m_shader_module; }
private:
	VkShaderModule m_shader_module = VK_NULL_HANDLE;
};

}
