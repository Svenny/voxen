#include <voxen/client/vulkan/high/render_graph_compiler.hpp>

#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

namespace voxen::client::vulkan
{

RenderGraphCompiler::ExpandedStageInfo::ExpandedStageInfo() noexcept
{
	color_attachments.fill(RenderTarget::None);
	resolve_attachments.fill(RenderTarget::None);
}

void RenderGraphCompiler::expandStage(RenderStage stage, const RenderGraph::StageProperties &props,
	RenderGraph::DynamicConfig config)
{
	if (props.dynamic_condition && !props.dynamic_condition(config)) {
		// Stage is not active in the given dynamic config
		return;
	}

	//ExpandedStageInfo &info = m_stage_info[stage];

	bool has_render_targets = false;

	for (const auto &req : props.requires_targets) {
		//RenderTarget target = req.target;
		if (req.type == RenderGraph::TargetReadType::InputAttachment ||
			req.type == RenderGraph::TargetReadType::ReadOnlyAttachment) {
			if (!has_render_targets) {
				has_render_targets = true;
			}
		}
	}

	/*for (const auto &prov : props.provides_targets) {
		RenderTarget target = prov.target;
	}*/

	if (!has_render_targets) {
		Log::error("Stage '{}' has no render targets, this is not supported yet", RenderGraph::getStageName(stage));
		throw MessageException("render graph compile error");
	}
}

/*
	auto is_target_active = [this](RenderTarget target, DynamicConfig config) {
		auto iter = m_target_props.find(target);
		if (iter == m_target_props.end()) {
			Log::error("Target '{}' has no description but is used", getTargetName(target));
			throw MessageException("render graph compile error");
		}

		if (iter->second.dynamic_condition) {
			return iter->second.dynamic_condition(config);
		}
		return true;
	};

	std::unordered_map<RenderTarget, RenderStage> provider_for_target;

	for (uint32_t stage_id = 1; stage_id < uint32_t(RenderStage::EnumMax); stage_id++) {
		RenderStage stage { stage_id };
		auto props_iter = m_stage_props.find(stage);
		if (props_iter == m_stage_props.end()) {
			// Stage has no description, so it is statically inactive
			continue;
		}
		const auto &stage_props = props_iter->second;

		bool has_render_targets = false;
		TargetDimensionsClass prov_dims;
		TargetSamplesClass prov_samples;

		uint32_t color_slot_occupancy_mask = 0;
		uint32_t resolve_slot_occupancy_mask = 0;
		RenderTarget zs_attachment = RenderTarget::None;
		RenderTarget zs_resolve_attachment = RenderTarget::None;

		for (const auto &prov : stage_props.provides_targets) {
			RenderTarget target = prov.target;
			if (!is_target_active(target, config)) {
				continue;
			}

			if (prov.type == TargetWriteType::Attachment) {
				if (prov.attachment_slot == ATTACHMENT_SLOT_NA) {
					Log::error("Stage '{}' uses target '{}' as attachment but doesn't define slot",
						getStageName(stage), getTargetName(target));
					throw MessageException("render graph compile error");
				} else if (prov.attachment_slot == ATTACHMENT_SLOT_ZS) {
					if (zs_attachment != RenderTarget::None) {
						Log::error("Stage '{}' uses multiple depth/stencil attachments", getStageName(stage));
						throw MessageException("render graph compile error");
					}
					zs_attachment = target;
				} else {
					assert(prov.attachment_slot < 32);
					uint32_t bit = 1u << prov.attachment_slot;
					if (color_slot_occupancy_mask & bit) {
						Log::error("Stage '{}' uses color attachment slot {} multiple times",
							getStageName(stage), prov.attachment_slot);
						throw MessageException("render graph compile error");
					}
					color_slot_occupancy_mask |= bit;
				}
			} else if (prov.type == TargetWriteType::ResolveAttachment) {
				if (prov.attachment_slot == ATTACHMENT_SLOT_NA) {
					Log::error("Stage '{}' uses target '{}' as resolve attachment but doesn't define slot",
						getStageName(stage), getTargetName(target));
					throw MessageException("render graph compile error");
				} else if (prov.attachment_slot == ATTACHMENT_SLOT_ZS) {
					if (zs_resolve_attachment != RenderTarget::None) {
						Log::error("Stage '{}' uses multiple depth/stencil resolve attachments", getStageName(stage));
						throw MessageException("render graph compile error");
					}
					zs_resolve_attachment = target;
				} else {
					assert(prov.attachment_slot < 32);
					uint32_t bit = 1u << prov.attachment_slot;
					if (resolve_slot_occupancy_mask & bit) {
						Log::error("Stage '{}' uses resolve attachment slot {} multiple times",
							getStageName(stage), prov.attachment_slot);
						throw MessageException("render graph compile error");
					}
					resolve_slot_occupancy_mask |= bit;
				}
			}

			if (auto[iter, res] = provider_for_target.emplace(target, stage); !res) {
				Log::error("Stage '{}' provides target '{}' which is already provided by stage '{}'",
					getStageName(stage), getTargetName(target), getStageName(iter->second));
				throw MessageException("render graph compile error");
			}

			if (prov.type == TargetWriteType::Attachment || prov.type == TargetWriteType::ResolveAttachment) {
				const TargetProperties &target_props = m_target_props.find(target)->second;

				if (!has_render_targets) {
					has_render_targets = true;
					prov_dims = target_props.dimensions;
					prov_samples = target_props.samples;
				} else {
					if (prov_dims != target_props.dimensions) {
						Log::error("Stage '{}' has contradicting provided targets dimensions", getStageName(stage));
						throw MessageException("render graph compile error");
					}
					if (prov_samples != target_props.samples) {
						Log::error("Stage '{}' has contradicting provided targets samples counts", getStageName(stage));
						throw MessageException("render graph compile error");
					}
				}
			}
		}
	}
*/

}
