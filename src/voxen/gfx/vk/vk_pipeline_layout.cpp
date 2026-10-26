#include <voxen/gfx/vk/vk_pipeline_layout.hpp>

#include <voxen/gfx/vk/vk_descriptor_set_layout.hpp>
#include <voxen/gfx/vk/vk_device.hpp>
#include <voxen/gfx/vk/vk_error.hpp>
#include <voxen/util/log.hpp>

#include <extras/defer.hpp>

#include <m/vector.hpp>

#include <cstdint>
#include <span>

namespace voxen::gfx::vk
{

namespace
{

VkPipelineLayout createLayout(Device &dev, std::span<const VkDescriptorSetLayout> set_layouts,
	std::span<const VkPushConstantRange> push_const_ranges)
{
	const VkPipelineLayoutCreateInfo info {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.setLayoutCount = uint32_t(set_layouts.size()),
		.pSetLayouts = set_layouts.data(),
		.pushConstantRangeCount = uint32_t(push_const_ranges.size()),
		.pPushConstantRanges = push_const_ranges.data(),
	};

	VkPipelineLayout handle = VK_NULL_HANDLE;
	VkResult result = dev.dt().vkCreatePipelineLayout(dev.handle(), &info, nullptr, &handle);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkCreatePipelineLayout");
	}

	return handle;
}

} // namespace

PipelineLayoutCollection::PipelineLayoutCollection(Device &dev, const DescriptorSetLayoutCollection &dsls) : m_dev(dev)
{
	defer_fail { destroyLayouts(); };

	const VkDescriptorSetLayout main_scene_dsl = dsls.mainSceneLayout();

	{
		const VkDescriptorSetLayout set_layouts[] = { main_scene_dsl, dsls.landFrustumCullLayout() };
		m_land_frustum_cull_layout = createLayout(dev, set_layouts, {});
	}

	{
		const VkDescriptorSetLayout set_layouts[] = { main_scene_dsl, dsls.landChunkMeshLayout() };
		m_land_chunk_mesh_layout = createLayout(dev, set_layouts, {});
	}

	{
		const VkPushConstantRange push_const_range {
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
			.offset = 0,
			.size = sizeof(m::float4),
		};
		m_land_selector_layout = createLayout(dev, { &main_scene_dsl, 1 }, { &push_const_range, 1 });
	}

	// Both UI layouts have screen size inverse as push constant
	const VkPushConstantRange ui_push_const_range {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.offset = 0,
		.size = sizeof(m::float2),
	};

	{
		const VkDescriptorSetLayout set_layout = dsls.uiFontLayout();
		m_ui_font_layout = createLayout(dev, { &set_layout, 1 }, { &ui_push_const_range, 1 });
	}

	{
		const VkDescriptorSetLayout set_layout = dsls.uiBasicLayout();
		m_ui_basic_layout = createLayout(dev, { &set_layout, 1 }, { &ui_push_const_range, 1 });
	}

	Log::debug("PipelineLayoutCollection created successfully");
}

PipelineLayoutCollection::~PipelineLayoutCollection() noexcept
{
	destroyLayouts();
}

void PipelineLayoutCollection::destroyLayouts() noexcept
{
	// Destroying null handles is allowed, no need to check
	VkDevice device = m_dev.handle();
	m_dev.dt().vkDestroyPipelineLayout(device, m_ui_basic_layout, nullptr);
	m_dev.dt().vkDestroyPipelineLayout(device, m_ui_font_layout, nullptr);
	m_dev.dt().vkDestroyPipelineLayout(device, m_land_selector_layout, nullptr);
	m_dev.dt().vkDestroyPipelineLayout(device, m_land_chunk_mesh_layout, nullptr);
	m_dev.dt().vkDestroyPipelineLayout(device, m_land_frustum_cull_layout, nullptr);
}

} // namespace voxen::gfx::vk
