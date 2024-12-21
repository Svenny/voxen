#pragma once

#include <voxen/gfx/vk/vk_include.hpp>

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
		LAND_DEBUG_CHUNK_BOUNDS_PIPELINE,
		LAND_CHUNK_MESH_PIPELINE,
		LAND_SELECTOR_PIPELINE,
		UI_FONT_PIPELINE,

		NUM_GRAPHICS_PIPELINES
	};

	enum ComputePipelineId : uint32_t {
		LAND_FRUSTUM_CULL_PIPELINE,

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
