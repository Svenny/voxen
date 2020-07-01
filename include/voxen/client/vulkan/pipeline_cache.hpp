#pragma once

#include <voxen/client/vulkan/common.hpp>

namespace voxen::client
{

class VulkanPipelineCache {
public:
	explicit VulkanPipelineCache(const char *path = nullptr);
	VulkanPipelineCache(VulkanPipelineCache &&) = delete;
	VulkanPipelineCache(const VulkanPipelineCache &) = delete;
	VulkanPipelineCache &operator = (VulkanPipelineCache &&) = delete;
	VulkanPipelineCache &operator = (const VulkanPipelineCache &) = delete;
	~VulkanPipelineCache() noexcept;

	// noexcept style is used because this function is automatically called in destructor
	bool dump() noexcept;

	operator VkPipelineCache() const noexcept { return m_cache; }
private:
	VkPipelineCache m_cache = VK_NULL_HANDLE;
	std::string m_save_path;
};

}
