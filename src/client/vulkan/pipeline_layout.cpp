#include <voxen/client/vulkan/pipeline_layout.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/descriptor_set_layout.hpp>
#include <voxen/gfx/vk/vk_device.hpp>
#include <voxen/gfx/vk/vk_error.hpp>
#include <voxen/util/log.hpp>

namespace voxen::client::vulkan
{

PipelineLayout::PipelineLayout(const VkPipelineLayoutCreateInfo &info)
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device().handle();
	VkResult result = backend.vkCreatePipelineLayout(device, &info, nullptr, &m_layout);
	if (result != VK_SUCCESS) {
		throw gfx::vk::VulkanException(result, "vkCreatePipelineLayout");
	}
}

PipelineLayout::~PipelineLayout() noexcept
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device().handle();
	backend.vkDestroyPipelineLayout(device, m_layout, nullptr);
}

PipelineLayoutCollection::PipelineLayoutCollection()
	: m_land_frustum_cull_layout(createLandFrustumCullLayout())
	, m_land_chunk_mesh_layout(createLandChunkMeshLayout())
	, m_land_selector_layout(createLandSelectorLayout())
	, m_ui_font_layout(createUiFontLayout())
{
	Log::debug("PipelineLayoutCollection created successfully");
}

PipelineLayout PipelineLayoutCollection::createLandFrustumCullLayout()
{
	auto &ds_collection = Backend::backend().descriptorSetLayoutCollection();

	VkDescriptorSetLayout layouts[2];
	layouts[0] = ds_collection.mainSceneLayout();
	layouts[1] = ds_collection.landFrustumCullLayout();

	return PipelineLayout(VkPipelineLayoutCreateInfo {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.setLayoutCount = std::size(layouts),
		.pSetLayouts = layouts,
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr,
	});
}

PipelineLayout PipelineLayoutCollection::createLandChunkMeshLayout()
{
	auto &ds_collection = Backend::backend().descriptorSetLayoutCollection();

	VkDescriptorSetLayout layouts[2];
	layouts[0] = ds_collection.mainSceneLayout();
	layouts[1] = ds_collection.landChunkMeshLayout();

	return PipelineLayout(VkPipelineLayoutCreateInfo {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.setLayoutCount = std::size(layouts),
		.pSetLayouts = layouts,
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr,
	});
}

PipelineLayout PipelineLayoutCollection::createLandSelectorLayout()
{
	auto &ds_collection = Backend::backend().descriptorSetLayoutCollection();

	VkDescriptorSetLayout layout = ds_collection.mainSceneLayout();

	VkPushConstantRange push_const_range {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.offset = 0,
		.size = sizeof(glm::vec4),
	};

	return PipelineLayout(VkPipelineLayoutCreateInfo {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.setLayoutCount = 1,
		.pSetLayouts = &layout,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &push_const_range,
	});
}

PipelineLayout PipelineLayoutCollection::createUiFontLayout()
{
	auto &ds_collection = Backend::backend().descriptorSetLayoutCollection();

	VkDescriptorSetLayout layout = ds_collection.uiFontLayout();

	VkPushConstantRange push_const_range {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.offset = 0,
		.size = sizeof(glm::vec2),
	};

	return PipelineLayout(VkPipelineLayoutCreateInfo {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.setLayoutCount = 1,
		.pSetLayouts = &layout,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &push_const_range,
	});
}

} // namespace voxen::client::vulkan
