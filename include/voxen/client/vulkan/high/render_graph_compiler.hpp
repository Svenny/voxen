#pragma once

#include <voxen/client/vulkan/capabilities.hpp>
#include <voxen/client/vulkan/high/render_graph.hpp>

#include <unordered_map>

namespace voxen::client::vulkan
{

class RenderGraphCompiler {
public:

private:
	struct ExpandedStageInfo {
		ExpandedStageInfo() noexcept;

		uint32_t render_pass_id = UINT32_MAX;
		uint32_t subpass_id = UINT32_MAX;

		RenderGraph::TargetDimensionsClass target_dims;
		RenderGraph::TargetSamplesClass target_samples;

		std::array<RenderTarget, Capabilities::NUM_REQUIRED_COLOR_ATTACHMENTS> input_attachments;
		std::array<RenderTarget, Capabilities::NUM_REQUIRED_COLOR_ATTACHMENTS> color_attachments;
		std::array<RenderTarget, Capabilities::NUM_REQUIRED_COLOR_ATTACHMENTS> resolve_attachments;
		RenderTarget zs_attachment = RenderTarget::None;
		RenderTarget zs_resolve_attachment = RenderTarget::None;
	};

	struct ExpandedTargetInfo {
		RenderStage provider = RenderStage::None;
		VkFormat format = VK_FORMAT_UNDEFINED;
		VkExtent3D dimensions = { UINT32_MAX, UINT32_MAX, UINT32_MAX };
		uint32_t levels = UINT32_MAX;
		uint32_t layers = UINT32_MAX;
		VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;
		uint32_t num_temporal_copies = UINT32_MAX;
	};

	std::unordered_map<RenderStage, ExpandedStageInfo> m_stage_info;
	std::unordered_map<RenderTarget, ExpandedTargetInfo> m_target_info;

	void expandStage(RenderStage stage, const RenderGraph::StageProperties &props, RenderGraph::DynamicConfig config);
};

}
