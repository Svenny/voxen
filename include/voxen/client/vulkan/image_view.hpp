#pragma once

#include <voxen/client/vulkan/command_buffer.hpp>

namespace voxen::client
{

class VulkanImageView {
public:
	VulkanImageView(const VkImageViewCreateInfo &info);
	VulkanImageView(VulkanImageView &&) = delete;
	VulkanImageView(const VulkanImageView &) = delete;
	VulkanImageView &operator = (VulkanImageView &&) = delete;
	VulkanImageView &operator = (const VulkanImageView &) = delete;
	~VulkanImageView() noexcept;

	operator VkImageView() const noexcept { return m_view; }
private:
	VkImageView m_view;
};

}
