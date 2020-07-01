#include <voxen/client/vulkan/pipeline_layout.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>

#include <voxen/util/log.hpp>

namespace voxen::client
{

VulkanPipelineLayout::VulkanPipelineLayout(const VkPipelineLayoutCreateInfo &info) {
	auto &backend = VulkanBackend::backend();
	VkDevice device = *backend.device();
	VkResult result = backend.vkCreatePipelineLayout(device, &info, VulkanHostAllocator::callbacks(), &m_layout);
	if (result != VK_SUCCESS)
		throw VulkanException(result);
}

VulkanPipelineLayout::~VulkanPipelineLayout() noexcept {
	auto &backend = VulkanBackend::backend();
	VkDevice device = *backend.device();
	backend.vkDestroyPipelineLayout(device, m_layout, VulkanHostAllocator::callbacks());
}

VulkanPipelineLayoutCollection::VulkanPipelineLayoutCollection()
	: m_descriptorless_layout(createDescriptorlessLayout())
{
	Log::debug("VulkanPipelineLayoutCollection created successfully");
}

VulkanPipelineLayout VulkanPipelineLayoutCollection::createDescriptorlessLayout() {
	VkPushConstantRange range;
	range.offset = 0;
	range.size = 128;
	range.stageFlags = VK_SHADER_STAGE_ALL;

	VkPipelineLayoutCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	info.pushConstantRangeCount = 1;
	info.pPushConstantRanges = &range;
	return VulkanPipelineLayout(info);
}

}
