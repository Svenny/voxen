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

	VkPipeline operator[](uint32_t idx) const noexcept { return m_graphics_pipelines[idx]; }

	enum GraphicsPipelineId : uint32_t {
		DEBUG_OCTREE_PIPELINE,

		NUM_GRAPHICS_PIPELINES
	};
private:
	VkPipeline m_graphics_pipelines[NUM_GRAPHICS_PIPELINES];

};

}
