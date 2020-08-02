#pragma once

#include <voxen/client/vulkan/common.hpp>

namespace voxen::client::vulkan
{

class PipelineLayout {
public:
	PipelineLayout(const VkPipelineLayoutCreateInfo &info);
	PipelineLayout(PipelineLayout &&) = delete;
	PipelineLayout(const PipelineLayout &) = delete;
	PipelineLayout &operator = (PipelineLayout &&) = delete;
	PipelineLayout &operator = (const PipelineLayout &) = delete;
	~PipelineLayout() noexcept;

	operator VkPipelineLayout() const noexcept { return m_layout; }
private:
	VkPipelineLayout m_layout = VK_NULL_HANDLE;
};

class PipelineLayoutCollection {
public:
	PipelineLayoutCollection();
	PipelineLayoutCollection(PipelineLayoutCollection &&) = delete;
	PipelineLayoutCollection(const PipelineLayoutCollection &) = delete;
	PipelineLayoutCollection &operator = (PipelineLayoutCollection &&) = delete;
	PipelineLayoutCollection &operator = (const PipelineLayoutCollection &) = delete;
	~PipelineLayoutCollection() = default;

	PipelineLayout &descriptorlessLayout() noexcept { return m_descriptorless_layout; }
private:
	PipelineLayout m_descriptorless_layout;

	PipelineLayout createDescriptorlessLayout();
};

}
