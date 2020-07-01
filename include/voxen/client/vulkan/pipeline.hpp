#pragma once

#include <voxen/client/vulkan/common.hpp>

namespace voxen::client
{

class VulkanPipelineCollection {
public:
	VulkanPipelineCollection();
	VulkanPipelineCollection(VulkanPipelineCollection &&) = delete;
	VulkanPipelineCollection(const VulkanPipelineCollection &) = delete;
	VulkanPipelineCollection &operator = (VulkanPipelineCollection &&) = delete;
	VulkanPipelineCollection &operator = (const VulkanPipelineCollection &) = delete;
	~VulkanPipelineCollection() noexcept;

	VkPipeline operator[](uint32_t idx) const noexcept { return m_graphics_pipelines[idx]; }

	enum GraphicsPipelineId : uint32_t {
		DEBUG_OCTREE_PIPELINE,

		NUM_GRAPHICS_PIPELINES
	};
private:
	VkPipeline m_graphics_pipelines[NUM_GRAPHICS_PIPELINES];

};

}
