#pragma once

#include <voxen/client/vulkan/common.hpp>

#include <array>

namespace voxen::client::vulkan
{

class PipelineCollection {
public:
	PipelineCollection();
	PipelineCollection(PipelineCollection &&) = delete;
	PipelineCollection(const PipelineCollection &) = delete;
	PipelineCollection &operator=(PipelineCollection &&) = delete;
	PipelineCollection &operator=(const PipelineCollection &) = delete;
	~PipelineCollection() noexcept;

	enum GraphicsPipelineId : uint32_t {
		DEBUG_OCTREE_PIPELINE,
		TERRAIN_SIMPLE_PIPELINE,

		NUM_GRAPHICS_PIPELINES
	};

	enum ComputePipelineId : uint32_t {
		TERRAIN_FRUSTUM_CULL_PIPELINE,

		NUM_COMPUTE_PIPELINES
	};

	VkPipeline operator[](GraphicsPipelineId idx) const noexcept;
	VkPipeline operator[](ComputePipelineId idx) const noexcept;

private:
	void destroyPipelines() noexcept;

	std::array<VkPipeline, NUM_GRAPHICS_PIPELINES> m_graphics_pipelines;
	std::array<VkPipeline, NUM_COMPUTE_PIPELINES> m_compute_pipelines;
};

} // namespace voxen::client::vulkan
