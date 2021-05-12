#include <voxen/client/vulkan/high/render_graph.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>
#include <voxen/client/vulkan/surface.hpp>

#include <voxen/util/log.hpp>

#include <algorithm>
#include <chrono>

namespace voxen::client::vulkan
{

void RenderGraph::rebuild(const GraphicsOptions &opts)
{
	Log::info("Started rebuilding the render graph");

	auto &backend = Backend::backend();
	// We are going to rebuild render passes, wait while existing ones become unused
	backend.device().waitIdle();

	auto t1 = std::chrono::steady_clock::now();

	// Iterate over all possible MainPassFlags combinations and create passes for them
	for (uint32_t flags = 0; flags < DYNAMIC_CONFIG_BIT_WIDTH; flags++) {
		createMainPass(opts, DynamicConfig(flags));
	}

	auto t2 = std::chrono::steady_clock::now();
	double took_ms = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(t2 - t1).count();
	Log::info("Render graph rebuilt in {:.2f} ms", took_ms);
}

void RenderGraph::createMainPass(const GraphicsOptions &opts, DynamicConfig config)
{
	const VkFormat output_format = Backend::backend().surface().format().format;

	VkSampleCountFlagBits num_samples;
	switch (opts.aaMethod()) {
	case GraphicsOptions::AaMethod::None:
		num_samples = VK_SAMPLE_COUNT_1_BIT;
		break;
	case GraphicsOptions::AaMethod::Msaa2x:
		num_samples = VK_SAMPLE_COUNT_2_BIT;
		break;
	case GraphicsOptions::AaMethod::Msaa4x:
		num_samples = VK_SAMPLE_COUNT_4_BIT;
		break;
	case GraphicsOptions::AaMethod::Msaa8x:
		num_samples = VK_SAMPLE_COUNT_8_BIT;
		break;
	default:
		Log::error("Requested unsupported AA method for main pass");
		throw MessageException("unsupported render graph");
	}

	// Set up render targets
	VkAttachmentDescription2 render_targets[5];
	// 0: HDR scene
	render_targets[0] = {
		.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
		.pNext = nullptr,
		.flags = 0,
		.format = SCENE_HDR_COLOR_FORMAT,
		.samples = num_samples,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};
	// 1: Scene Z
	render_targets[1] = {
		.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
		.pNext = nullptr,
		.flags = 0,
		.format = SCENE_DEPTH_FORMAT,
		.samples = num_samples,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
	};
	// 2: Output swapchain image
	render_targets[2] = {
		.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
		.pNext = nullptr,
		.flags = 0,
		.format = output_format,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	};
	// 3: OIT color accumulator [if has transparency]
	render_targets[3] = {
		.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
		.pNext = nullptr,
		.flags = 0,
		.format = OIT_ACCUM_FORMAT,
		// OIT pass does not use multisampling
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};
	// 4: OIT reveal accumulator [if has transparency]
	render_targets[4] = {
		.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
		.pNext = nullptr,
		.flags = 0,
		.format = OIT_REVEAL_FORMAT,
		// OIT pass does not use multisampling
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};

	// Subpass 0 attachment references
	const VkAttachmentReference2 sp0_color_ref {
		.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
		.pNext = nullptr,
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
	};
	const VkAttachmentReference2 sp0_depth_ref {
		.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
		.pNext = nullptr,
		.attachment = 1,
		.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT
	};

	// Subpass 1 attachment references
	VkAttachmentReference2 sp1_color_refs[2];
	sp1_color_refs[0] = {
		.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
		.pNext = nullptr,
		.attachment = 3,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
	};
	sp1_color_refs[1] = {
		.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
		.pNext = nullptr,
		.attachment = 4,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
	};
	const VkAttachmentReference2 sp1_depth_ref {
		.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
		.pNext = nullptr,
		.attachment = 1,
		.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT
	};
	const uint32_t sp1_preserve_ref = 0;

	// Subpass 2 attachment references
	VkAttachmentReference2 sp2_input_refs[3];
	sp2_input_refs[0] = {
		.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
		.pNext = nullptr,
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
	};
	sp2_input_refs[1] = {
		.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
		.pNext = nullptr,
		.attachment = 3,
		.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
	};
	sp2_input_refs[2] = {
		.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
		.pNext = nullptr,
		.attachment = 4,
		.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
	};
	const VkAttachmentReference2 sp2_color_ref {
		.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
		.pNext = nullptr,
		.attachment = 2,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
	};

	// Set up subpasses
	VkSubpassDescription2 subpasses[3];
	// 0: HDR scene subpass
	subpasses[0] = {
		.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
		.pNext = nullptr,
		.flags = 0,
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.viewMask = 0,
		.inputAttachmentCount = 0,
		.pInputAttachments = nullptr,
		.colorAttachmentCount = 1,
		.pColorAttachments = &sp0_color_ref,
		.pResolveAttachments = nullptr,
		.pDepthStencilAttachment = &sp0_depth_ref,
		.preserveAttachmentCount = 0,
		.pPreserveAttachments = nullptr
	};
	// 1: OIT subpass
	subpasses[1] = {
		.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
		.pNext = nullptr,
		.flags = 0,
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.viewMask = 0,
		.inputAttachmentCount = 0,
		.pInputAttachments = nullptr,
		.colorAttachmentCount = 2,
		.pColorAttachments = sp1_color_refs,
		.pResolveAttachments = nullptr,
		.pDepthStencilAttachment = &sp1_depth_ref,
		.preserveAttachmentCount = 1,
		.pPreserveAttachments = &sp1_preserve_ref
	};
	// 2: Tonemap/MSAA resolve/OIT resolve/UI subpass
	subpasses[2] = {
		.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
		.pNext = nullptr,
		.flags = 0,
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.viewMask = 0,
		.inputAttachmentCount = 3,
		.pInputAttachments = sp2_input_refs,
		.colorAttachmentCount = 1,
		.pColorAttachments = &sp2_color_ref,
		.pResolveAttachments = nullptr,
		.pDepthStencilAttachment = nullptr,
		.preserveAttachmentCount = 0,
		.pPreserveAttachments = nullptr
	};

	VkSubpassDependency2 dependencies[3];
	dependencies[0] = {
		.sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
		.pNext = nullptr,
		.srcSubpass = 0,
		.dstSubpass = 2,
		.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
		.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
		.viewOffset = 0
	};
	dependencies[1] = {
		.sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
		.pNext = nullptr,
		.srcSubpass = 0,
		.dstSubpass = 1,
		.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
		.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
		.viewOffset = 0
	};
	dependencies[2] = {
		.sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
		.pNext = nullptr,
		.srcSubpass = 1,
		.dstSubpass = 2,
		.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
		.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
		.viewOffset = 0
	};

	VkRenderPassCreateInfo2 info{};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2;

	if (config & DYNAMIC_CONFIG_HAS_FULL_RES_TRANSPARENCY_BIT) {
		info.attachmentCount = 5;
		info.subpassCount = 3;
		info.dependencyCount = 3;
	} else {
		info.attachmentCount = 3;
		info.subpassCount = 2;
		info.dependencyCount = 1;
		// Make resolve/UI the last subpass
		subpasses[1] = subpasses[2];
		subpasses[1].inputAttachmentCount = 1;
		dependencies[0].dstSubpass = 1;
	}

	info.pAttachments = render_targets;
	info.pSubpasses = subpasses;
	info.pDependencies = dependencies;

	m_main_pass[config] = RenderPass(info);
}

void RenderGraph::fillRenderGraph(const GraphicsOptions &opts)
{
	m_target_props.clear();
	m_stage_props.clear();

	bool has_samples = false;

	switch (opts.aaMethod()) {
	case GraphicsOptions::AaMethod::None:
	case GraphicsOptions::AaMethod::Taa1S2T:
	case GraphicsOptions::AaMethod::Taa1S4T:
	case GraphicsOptions::AaMethod::Taa1S8T:
		has_samples = false;
		break;
	case GraphicsOptions::AaMethod::Msaa2x:
	case GraphicsOptions::AaMethod::Msaa4x:
	case GraphicsOptions::AaMethod::Msaa8x:
		has_samples = true;
		break;
	default:
		Log::error("Requested unsupported AA method for main pass");
		throw MessageException("unsupported render graph");
	}

	auto dyn_condition_full_res_oit = [](DynamicConfig config) noexcept {
		return (config & DYNAMIC_CONFIG_HAS_FULL_RES_TRANSPARENCY_BIT) != 0;
	};
	auto dyn_condition_half_res_oit = [](DynamicConfig config) noexcept {
		return (config & DYNAMIC_CONFIG_HAS_HALF_RES_TRANSPARENCY_BIT) != 0;
	};



	uint32_t order_counter = 0;
	{ // Scene HDR draw pass
		auto &props = m_stage_props[RenderStage::SceneHdrDrawPass];
		props.order = order_counter++;
		// Provides
		if (has_samples) {
			props.provides_targets.push_back({ .target = RenderTarget::SceneHdrColorSamples,
				.type = TargetWriteType::Attachment, .attachment_slot = 0 });
			props.provides_targets.push_back({ .target = RenderTarget::SceneDepthStencilSamples,
				.type = TargetWriteType::Attachment, .attachment_slot = ATTACHMENT_SLOT_ZS });
			props.provides_targets.push_back({ .target = RenderTarget::SceneDepthStencilResolved,
				.type = TargetWriteType::ResolveAttachment, .attachment_slot = ATTACHMENT_SLOT_ZS });
		} else {
			props.provides_targets.push_back({ .target = RenderTarget::SceneHdrColorResolved,
				.type = TargetWriteType::Attachment, .attachment_slot = 0 });
			props.provides_targets.push_back({ .target = RenderTarget::SceneDepthStencilResolved,
				.type = TargetWriteType::Attachment, .attachment_slot = ATTACHMENT_SLOT_ZS });
		}
	}
	if (has_samples) { // Scene HDR resolve pass
		auto &props = m_stage_props[RenderStage::SceneHdrResolvePass];
		props.order = order_counter++;
		// Requires
		props.requires_targets.push_back({ .target = RenderTarget::SceneHdrColorSamples,
			.type = TargetReadType::InputAttachment, .attachment_slot = 0 });
		// Provides
		props.provides_targets.push_back({ .target = RenderTarget::SceneHdrColorResolved,
			.type = TargetWriteType::Attachment, .attachment_slot = 0 });
	}
	{ // OIT full resolution pass
		auto &props = m_stage_props[RenderStage::OitFullResPass];
		props.dynamic_condition = dyn_condition_full_res_oit;
		props.order = order_counter++;
		// Requires
		props.requires_targets.push_back({ .target = RenderTarget::SceneDepthStencilResolved,
			.type = TargetReadType::ReadOnlyAttachment, .attachment_slot = ATTACHMENT_SLOT_ZS });
		// Provides
		props.provides_targets.push_back({ .target = RenderTarget::OitColor,
			.type = TargetWriteType::Attachment, .attachment_slot = 0 });
		props.provides_targets.push_back({ .target = RenderTarget::OitReveal,
			.type = TargetWriteType::Attachment, .attachment_slot = 1 });
	}
	{ // OIT half resolution pass
		auto &props = m_stage_props[RenderStage::OitHalfResPass];
		props.dynamic_condition = dyn_condition_half_res_oit;
		props.order = order_counter++;
		// Requires
		props.requires_targets.push_back({ .target = RenderTarget::SceneDepthStencilResolved,
			.type = TargetReadType::ReadOnlyAttachment, .attachment_slot = ATTACHMENT_SLOT_ZS, .mip_level = 1 });
		// Provides
		props.provides_targets.push_back({ .target = RenderTarget::OitColor,
			.type = TargetWriteType::Attachment, .attachment_slot = 0 });
		props.provides_targets.push_back({ .target = RenderTarget::OitReveal,
			.type = TargetWriteType::Attachment, .attachment_slot = 1 });
	}
	{ // OIT+HDR resolve pass
		auto &props = m_stage_props[RenderStage::HdrResolvePass];
		props.order = order_counter++;
		// Requires
		props.requires_targets.push_back({ .target = RenderTarget::SceneHdrColorResolved,
			.type = TargetReadType::InputAttachment, .attachment_slot = 0 });
		props.requires_targets.push_back({ .target = RenderTarget::OitColor,
			.type = TargetReadType::InputAttachment, .attachment_slot = 1 });
		props.requires_targets.push_back({ .target = RenderTarget::OitReveal,
			.type = TargetReadType::InputAttachment, .attachment_slot = 2 });
		props.requires_targets.push_back({ .target = RenderTarget::OitColor,
			.type = TargetReadType::SampledImage });
		props.requires_targets.push_back({ .target = RenderTarget::OitReveal,
			.type = TargetReadType::SampledImage });
		// Provides
		props.provides_targets.push_back({ .target = RenderTarget::SceneFinal,
			.type = TargetWriteType::Attachment, .attachment_slot = 0 });
	}
	{ // Final composition/postprocessing/UI pass
		auto &props = m_stage_props[RenderStage::FinalPass];
		props.order = order_counter++;
		// Requires
		props.requires_targets.push_back({ .target = RenderTarget::SceneFinal,
			.type = TargetReadType::SampledImage });
		// Provides
		props.provides_targets.push_back({ .target = RenderTarget::Swapchain,
			.type = TargetWriteType::Attachment, .attachment_slot = 0 });
	}
}

const char *RenderGraph::getStageName(RenderStage stage) noexcept
{
	switch (stage) {
	case RenderStage::None: return "None";
	case RenderStage::SceneHdrDrawPass: return "SceneHdrDrawPass";
	case RenderStage::SceneHdrResolvePass: return "SceneHdrResolvePass";
	case RenderStage::OitFullResPass: return "OitFullResPass";
	case RenderStage::OitHalfResPass: return "OitHalfResPass";
	case RenderStage::HdrResolvePass: return "HdrResolvePass";
	case RenderStage::FinalPass: return "FinalPass";
	default: return "[UNKNOWN]";
	}
}

}
