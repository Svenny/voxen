#pragma once

#include <voxen/client/vulkan/capabilities.hpp>
#include <voxen/client/vulkan/high/render_graph.hpp>

namespace voxen::client::vulkan
{

class RenderGraphBuilder {
public:

	struct StageInfo {
		RenderGraph::DynamicCondition dynamic_condition = nullptr;
	};

	struct TargetInfo {
		RenderGraph::DynamicCondition dynamic_condition = nullptr;
		RenderGraph::TargetFormatClass format = RenderGraph::TargetFormatClass::Invalid;
		RenderGraph::TargetDimensionsClass dimensions = RenderGraph::TargetDimensionsClass::Invalid;
		RenderGraph::TargetSamplesClass samples = RenderGraph::TargetSamplesClass::One;
	};

	void addStage(RenderStage stage, const StageInfo &info);
	void addTarget(RenderTarget target, const TargetInfo &info);

	struct TargetReference {
		RenderTarget target = RenderTarget::None;
		uint32_t mip_level = 0;
		uint32_t array_level = 0;
		uint32_t min_temporal_offset = 0;
		uint32_t max_temporal_offset = 0;
		bool read_only = false;
	};

	void setInputAttachment(RenderStage stage, uint32_t slot, const TargetReference &ref);
	void setColorAttachment(RenderStage stage, uint32_t slot, const TargetReference &ref);
	void setResolveAttachment(RenderStage stage, uint32_t slot, const TargetReference &ref);
	void setDepthStencilAttachment(RenderStage stage, const TargetReference &ref);
	void setDepthStencilResolveAttachment(RenderStage stage, const TargetReference &ref);

	void addNonRenderRequirement(RenderStage stage, const TargetReference &ref);
	void addNonRenderProvidement(RenderStage stage, const TargetReference &ref);

private:

	struct InternalStageInfo {
		RenderGraph::DynamicCondition dynamic_condition = nullptr;
		uint32_t order = UINT32_MAX;

		RenderGraph::TargetDimensionsClass target_dims = RenderGraph::TargetDimensionsClass::Invalid;
		RenderGraph::TargetSamplesClass target_samples = RenderGraph::TargetSamplesClass::Invalid;

		std::array<TargetReference, Capabilities::NUM_REQUIRED_COLOR_ATTACHMENTS> color_attachments;
		TargetReference zs_attachment;

		std::array<TargetReference, Capabilities::NUM_REQUIRED_COLOR_ATTACHMENTS> input_attachments;
		std::array<TargetReference, Capabilities::NUM_REQUIRED_COLOR_ATTACHMENTS> resolve_attachments;
		TargetReference zs_resolve_attachment;
	};

	struct InternalTargetInfo {
		RenderGraph::DynamicCondition dynamic_condition = nullptr;
		RenderStage provider = RenderStage::None;

		RenderGraph::TargetFormatClass format = RenderGraph::TargetFormatClass::Invalid;
		RenderGraph::TargetDimensionsClass dimensions = RenderGraph::TargetDimensionsClass::Invalid;
		RenderGraph::TargetSamplesClass samples = RenderGraph::TargetSamplesClass::Invalid;

		uint32_t levels = 0;
		uint32_t layers = 0;
		uint32_t num_temporal_copies = 0;
	};

	std::unordered_map<RenderStage, InternalStageInfo> m_stage_info;
	std::unordered_map<RenderTarget, InternalTargetInfo> m_target_info;

	void ensureStageAdded(RenderStage stage) const;
	void ensureTargetAdded(RenderTarget target) const;
	void ensureSlotValidity(uint32_t slot) const;

	void updateRenderDimensions(RenderStage stage, RenderTarget target);
	void updateRenderSamples(RenderStage stage, RenderTarget target);

	void ensureNoDoubleTargetUsage(RenderStage stage, RenderTarget target) const;
};

}
