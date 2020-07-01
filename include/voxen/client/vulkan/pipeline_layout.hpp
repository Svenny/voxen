#pragma once

#include <voxen/client/vulkan/common.hpp>

namespace voxen::client
{

class VulkanPipelineLayout {
public:
	VulkanPipelineLayout(const VkPipelineLayoutCreateInfo &info);
	VulkanPipelineLayout(VulkanPipelineLayout &&) = delete;
	VulkanPipelineLayout(const VulkanPipelineLayout &) = delete;
	VulkanPipelineLayout &operator = (VulkanPipelineLayout &&) = delete;
	VulkanPipelineLayout &operator = (const VulkanPipelineLayout &) = delete;
	~VulkanPipelineLayout() noexcept;

	operator VkPipelineLayout() const noexcept { return m_layout; }
private:
	VkPipelineLayout m_layout = VK_NULL_HANDLE;
};

class VulkanPipelineLayoutCollection {
public:
	VulkanPipelineLayoutCollection();
	VulkanPipelineLayoutCollection(VulkanPipelineLayoutCollection &&) = delete;
	VulkanPipelineLayoutCollection(const VulkanPipelineLayoutCollection &) = delete;
	VulkanPipelineLayoutCollection &operator = (VulkanPipelineLayoutCollection &&) = delete;
	VulkanPipelineLayoutCollection &operator = (const VulkanPipelineLayoutCollection &) = delete;
	~VulkanPipelineLayoutCollection() = default;

	VulkanPipelineLayout &descriptorlessLayout() noexcept { return m_descriptorless_layout; }
private:
	VulkanPipelineLayout m_descriptorless_layout;

	VulkanPipelineLayout createDescriptorlessLayout();
};

}
