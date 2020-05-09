#pragma once

#include <vulkan/vulkan.h>

namespace voxen::client
{

class VulkanShader {
public:
	explicit VulkanShader(VkDevice dev) noexcept;
	~VulkanShader() noexcept;

	bool load(const char *path);

	operator VkShaderModule() const noexcept { return m_module; }
private:
	VkDevice m_dev = VK_NULL_HANDLE;

	VkShaderModule m_module = VK_NULL_HANDLE;
};

}
