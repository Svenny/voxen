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

	enum GraphicsPipeline : uint32_t {
		GraphicsPipelineLandDebugChunkBounds,
		GraphicsPipelineLandChunkMesh,
		GraphicsPipelineLandSelector,
		GraphicsPipelineUiFont,
		GraphicsPipelineUiBasic,

		GraphicsPipelineCount
	};

	enum ComputePipeline : uint32_t {
		ComputePipelineLandFrustumCull,

		ComputePipelineCount
	};

	VkPipeline operator[](GraphicsPipeline idx) const noexcept;
	VkPipeline operator[](ComputePipeline idx) const noexcept;

private:
	Device &m_dev;

	void destroyPipelines() noexcept;

	std::array<VkPipeline, GraphicsPipelineCount> m_graphics_pipelines;
	std::array<VkPipeline, ComputePipelineCount> m_compute_pipelines;
};

} // namespace voxen::gfx::vk
