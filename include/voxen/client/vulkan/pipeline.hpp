#pragma once

#include <voxen/client/vulkan/common.hpp>

namespace voxen::client::vulkan
{

class PipelineCollection {
public:
	PipelineCollection();
	PipelineCollection(PipelineCollection &&) = delete;
	PipelineCollection(const PipelineCollection &) = delete;
	PipelineCollection &operator = (PipelineCollection &&) = delete;
	PipelineCollection &operator = (const PipelineCollection &) = delete;
	~PipelineCollection() noexcept;

	enum GraphicsPipelineId : uint32_t {
		DEBUG_OCTREE_PIPELINE,
		TERRAIN_SIMPLE_PIPELINE,

		NUM_GRAPHICS_PIPELINES
	};

	VkPipeline operator[](GraphicsPipelineId idx) const;
private:
	std::array<VkPipeline, NUM_GRAPHICS_PIPELINES> m_graphics_pipelines;
};

}
