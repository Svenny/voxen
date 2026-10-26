#pragma once

#include <voxen/gfx/gfx_fwd.hpp>
#include <voxen/gfx/vk/vk_include.hpp>

#include <array>
#include <cstdint>

namespace voxen::gfx::vk
{

// Hardcoded set of pipelines used by legacy render passes.
// TODO: legacy part, pipelines should be created by their users or by a pipeline factory.
class PipelineCollection {
public:
	// Graphics pipelines are created to render into `color_format` attachment
	explicit PipelineCollection(Device &dev, const PipelineCache &cache, const PipelineLayoutCollection &layouts,
		VkFormat color_format);
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
		UI_BASIC_PIPELINE,

		NUM_GRAPHICS_PIPELINES
	};

	enum ComputePipelineId : uint32_t {
		LAND_FRUSTUM_CULL_PIPELINE,

		NUM_COMPUTE_PIPELINES
	};

	VkPipeline operator[](GraphicsPipelineId idx) const noexcept;
	VkPipeline operator[](ComputePipelineId idx) const noexcept;

private:
	Device &m_dev;

	void destroyPipelines() noexcept;

	std::array<VkPipeline, NUM_GRAPHICS_PIPELINES> m_graphics_pipelines;
	std::array<VkPipeline, NUM_COMPUTE_PIPELINES> m_compute_pipelines;
};

} // namespace voxen::gfx::vk
