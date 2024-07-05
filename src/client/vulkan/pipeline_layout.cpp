#include <voxen/client/vulkan/pipeline_layout.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/descriptor_set_layout.hpp>
#include <voxen/gfx/vk/vk_device.hpp>

#include <voxen/util/log.hpp>

namespace voxen::client::vulkan
{

PipelineLayout::PipelineLayout(const VkPipelineLayoutCreateInfo &info)
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device().handle();
	VkResult result = backend.vkCreatePipelineLayout(device, &info, HostAllocator::callbacks(), &m_layout);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkCreatePipelineLayout");
	}
}

PipelineLayout::~PipelineLayout() noexcept
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device().handle();
	backend.vkDestroyPipelineLayout(device, m_layout, HostAllocator::callbacks());
}

PipelineLayoutCollection::PipelineLayoutCollection()
	: m_terrain_basic_layout(createTerrainBasicLayout()), m_terrain_frustum_cull_layout(createTerrainFrustumCullLayout())
{
	Log::debug("PipelineLayoutCollection created successfully");
}

PipelineLayout PipelineLayoutCollection::createTerrainBasicLayout()
{
	auto &ds_collection = Backend::backend().descriptorSetLayoutCollection();
	VkDescriptorSetLayout layout = ds_collection.mainSceneLayout();

	constexpr VkPushConstantRange range {
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
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
		.pPushConstantRanges = &range,
	});
}

PipelineLayout PipelineLayoutCollection::createTerrainFrustumCullLayout()
{
	auto &ds_collection = Backend::backend().descriptorSetLayoutCollection();

	VkDescriptorSetLayout layouts[2];
	layouts[0] = ds_collection.mainSceneLayout();
	layouts[1] = ds_collection.terrainFrustumCullLayout();

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

} // namespace voxen::client::vulkan
