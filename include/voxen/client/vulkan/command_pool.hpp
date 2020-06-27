#pragma once

#include <voxen/client/vulkan/common.hpp>
#include <voxen/client/vulkan/command_buffer.hpp>

#include <extras/dyn_array.hpp>

namespace voxen::client
{

class VulkanCommandPool {
public:
	explicit VulkanCommandPool(uint32_t queue_family);
	VulkanCommandPool(VulkanCommandPool &&) = delete;
	VulkanCommandPool(const VulkanCommandPool &) = delete;
	VulkanCommandPool& operator = (VulkanCommandPool &&) = delete;
	VulkanCommandPool& operator = (const VulkanCommandPool &) = delete;
	~VulkanCommandPool() noexcept;

	extras::dyn_array<VulkanCommandBuffer> allocateCommandBuffers(uint32_t count, bool secondary = false);
	void freeCommandBuffers(extras::dyn_array<VulkanCommandBuffer> &buffers);
	void trim() noexcept;
	void reset(bool release_resources = false);

	operator VkCommandPool() const noexcept { return m_cmd_pool; }
private:
	VkCommandPool m_cmd_pool = VK_NULL_HANDLE;
};

}
