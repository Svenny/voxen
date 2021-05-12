#include <voxen/client/vulkan/high/render_graph_builder.hpp>

#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

namespace voxen::client::vulkan
{

constexpr static const char EXCEPTION_MESSAGE[] = "invalid call to RenderGraphBuilder";

void RenderGraphBuilder::addStage(RenderStage stage, const StageInfo &info)
{
	auto iter = m_stage_info.find(stage);
	if (iter != m_stage_info.end()) {
		Log::error("Attempt to double-add stage '{}'", RenderGraph::getStageName(stage));
		throw MessageException(EXCEPTION_MESSAGE);
	}

	auto &internal_info = m_stage_info[stage];
	internal_info.dynamic_condition = info.dynamic_condition;
	internal_info.order = m_stage_info.size() - 1;
}

void RenderGraphBuilder::addTarget(RenderTarget target, const TargetInfo &info)
{
	auto iter = m_target_info.find(target);
	if (iter != m_target_info.end()) {
		Log::error("Attempt to double-add target '{}'", getTargetName(target));
		throw MessageException(EXCEPTION_MESSAGE);
	}

	auto &internal_info = m_target_info[target];
	internal_info.dynamic_condition = info.dynamic_condition;
	internal_info.format = info.format;
	internal_info.dimensions = info.dimensions;
	internal_info.samples = info.samples;
}

void RenderGraphBuilder::setInputAttachment(RenderStage stage, uint32_t slot, const TargetReference &ref)
{
	ensureStageAdded(stage);
	ensureTargetAdded(ref.target);
	ensureSlotValidity(slot);

	//auto &stage_info = m_stage_info[stage];
	//auto &target_info = m_target_info[ref.target];

	if (!ref.read_only) {
		Log::error("Input attachment #'{}' for stage '{}' target '{}' was marked as non-read-only",
		   slot, RenderGraph::getStageName(stage), getTargetName(ref.target));
		throw MessageException(EXCEPTION_MESSAGE);
	}

	updateRenderDimensions(stage, ref.target);

	ensureNoDoubleTargetUsage(stage, ref.target);

}

void RenderGraphBuilder::setColorAttachment(RenderStage stage, uint32_t slot, const TargetReference &ref)
{
	ensureStageAdded(stage);
	ensureTargetAdded(ref.target);
	ensureSlotValidity(slot);
	ensureNoDoubleTargetUsage(stage, ref.target);

}

void RenderGraphBuilder::setResolveAttachment(RenderStage stage, uint32_t slot, const TargetReference &ref)
{
	ensureStageAdded(stage);
	ensureTargetAdded(ref.target);
	ensureSlotValidity(slot);
	ensureNoDoubleTargetUsage(stage, ref.target);

}

void RenderGraphBuilder::setDepthStencilAttachment(RenderStage stage, const TargetReference &ref)
{
	ensureStageAdded(stage);
	ensureTargetAdded(ref.target);
	ensureNoDoubleTargetUsage(stage, ref.target);

}

void RenderGraphBuilder::setDepthStencilResolveAttachment(RenderStage stage, const TargetReference &ref)
{
	ensureStageAdded(stage);
	ensureTargetAdded(ref.target);
	ensureNoDoubleTargetUsage(stage, ref.target);

}

void RenderGraphBuilder::ensureStageAdded(RenderStage stage) const
{
	auto iter = m_stage_info.find(stage);
	if (iter == m_stage_info.end()) {
		Log::error("Stage '{}' was not added to the builder but is being used", RenderGraph::getStageName(stage));
		throw MessageException(EXCEPTION_MESSAGE);
	}
}

void RenderGraphBuilder::ensureTargetAdded(RenderTarget target) const
{
	auto iter = m_target_info.find(target);
	if (iter == m_target_info.end()) {
		Log::error("Target '{}' was not added to the builder but is being used", getTargetName(target));
		throw MessageException(EXCEPTION_MESSAGE);
	}
}

void RenderGraphBuilder::ensureSlotValidity(uint32_t slot) const
{
	if (slot >= Capabilities::NUM_REQUIRED_COLOR_ATTACHMENTS) {
		Log::error("Tried to use render target slot '{}' but only '{}' are available",
			slot, Capabilities::NUM_REQUIRED_COLOR_ATTACHMENTS);
		throw MessageException(EXCEPTION_MESSAGE);
	}
}

void RenderGraphBuilder::updateRenderDimensions(RenderStage stage, RenderTarget target)
{
	// Externally guaranteed that both exist
	auto &stage_info = m_stage_info.find(stage)->second;
	auto &target_info = m_target_info.find(target)->second;

	if (stage_info.target_dims == RenderGraph::TargetDimensionsClass::Invalid) {
		stage_info.target_dims = target_info.dimensions;
	} else if (stage_info.target_dims != target_info.dimensions) {
		Log::error("Stage '{}' target '{}' has conflicting dimension requirements",
		   RenderGraph::getStageName(stage), getTargetName(target));
		throw MessageException(EXCEPTION_MESSAGE);
	}
}

void RenderGraphBuilder::updateRenderSamples(RenderStage stage, RenderTarget target)
{
	// Externally guaranteed that both exist
	auto &stage_info = m_stage_info.find(stage)->second;
	auto &target_info = m_target_info.find(target)->second;

	if (stage_info.target_samples == RenderGraph::TargetSamplesClass::Invalid) {
		stage_info.target_samples = target_info.samples;
	} else if (stage_info.target_samples != target_info.samples) {
		Log::error("Stage '{}' target '{}' has conflicting samples requirements",
		   RenderGraph::getStageName(stage), getTargetName(target));
		throw MessageException(EXCEPTION_MESSAGE);
	}
}

void RenderGraphBuilder::ensureNoDoubleTargetUsage(RenderStage stage, RenderTarget target) const
{
	// Externally guaranteed that both exist
	auto &stage_info = m_stage_info.find(stage)->second;
	auto &target_info = m_target_info.find(target)->second;

	if (target_info.provider != RenderStage::None) {
		Log::error("Stage '{}' attempts to provide target '{}', but it is already provided by stage '{}'",
		   RenderGraph::getStageName(stage), getTargetName(target),
			RenderGraph::getStageName(target_info.provider));
		throw MessageException(EXCEPTION_MESSAGE);
	}

	for (size_t i = 0; i < Capabilities::NUM_REQUIRED_COLOR_ATTACHMENTS; i++) {
		if (stage_info.color_attachments[i].target == target && !stage_info.color_attachments[i].read_only) {

		}
		if (stage_info.resolve_attachments[i].target == target) {

		}
	}
	for (const auto &ref : stage_info.color_attachments) {
		if (ref.target == target) {

		}
	}
}

}
