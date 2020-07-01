#include <voxen/client/vulkan/pipeline.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>
#include <voxen/client/vulkan/pipeline_cache.hpp>
#include <voxen/client/vulkan/pipeline_layout.hpp>
#include <voxen/client/vulkan/render_pass.hpp>
#include <voxen/client/vulkan/shader_module.hpp>
#include <voxen/client/vulkan/swapchain.hpp>

#include <voxen/util/assert.hpp>
#include <voxen/util/log.hpp>

namespace voxen::client
{

struct VertexFormatPositionOnly {
	VertexFormatPositionOnly() noexcept {
		vertex_input_binding.binding = 0;
		vertex_input_binding.stride = sizeof(float) * 3;
		vertex_input_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		vertex_input_attrib.binding = 0;
		vertex_input_attrib.location = 0;
		vertex_input_attrib.format = VK_FORMAT_R32G32B32_SFLOAT;
		vertex_input_attrib.offset = 0;

		vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertex_input_info.vertexBindingDescriptionCount = 1;
		vertex_input_info.pVertexBindingDescriptions = &vertex_input_binding;
		vertex_input_info.vertexAttributeDescriptionCount = 1;
		vertex_input_info.pVertexAttributeDescriptions = &vertex_input_attrib;
	}

	VkVertexInputBindingDescription vertex_input_binding = {};
	VkVertexInputAttributeDescription vertex_input_attrib = {};
	VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
};

struct DefaultViewportState {
	DefaultViewportState() noexcept {
		auto &backend = VulkanBackend::backend();
		auto *swapchain = backend.swapchain();
		vxAssert(swapchain != nullptr);

		VkExtent2D frame_size = swapchain->surfaceExtent();

		scissor.offset = { 0, 0 };
		scissor.extent = frame_size;

		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = float(frame_size.width);
		viewport.height = float(frame_size.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_info.scissorCount = 1;
		viewport_info.pScissors = &scissor;
		viewport_info.viewportCount = 1;
		viewport_info.pViewports = &viewport;
	}

	VkRect2D scissor;
	VkViewport viewport;
	VkPipelineViewportStateCreateInfo viewport_info = {};
};

struct DefaultRasterizationState {
	DefaultRasterizationState() noexcept {
		rasterization_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterization_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterization_info.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterization_info.polygonMode = VK_POLYGON_MODE_FILL;
		rasterization_info.lineWidth = 1.0f;
		rasterization_info.depthBiasEnable = VK_FALSE;
		rasterization_info.depthClampEnable = VK_FALSE;
		rasterization_info.rasterizerDiscardEnable = VK_FALSE;
	}

	VkPipelineRasterizationStateCreateInfo rasterization_info = {};
};

struct DisabledMsaaState {
	DisabledMsaaState() noexcept {
		multisample_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisample_info.sampleShadingEnable = VK_FALSE;
		multisample_info.minSampleShading = 1.0f;
		multisample_info.alphaToCoverageEnable = VK_FALSE;
		multisample_info.alphaToOneEnable = VK_FALSE;
	}

	VkPipelineMultisampleStateCreateInfo multisample_info = {};
};

struct DefaultDepthStencilState {
	DefaultDepthStencilState() noexcept {
		depth_stencil_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depth_stencil_info.depthTestEnable = VK_TRUE;
		depth_stencil_info.depthWriteEnable = VK_TRUE;
		depth_stencil_info.depthCompareOp = VK_COMPARE_OP_GREATER;
		depth_stencil_info.depthBoundsTestEnable = VK_FALSE;
		depth_stencil_info.stencilTestEnable = VK_FALSE;
		depth_stencil_info.minDepthBounds = 0.0f;
		depth_stencil_info.maxDepthBounds = 1.0f;
	}

	VkPipelineDepthStencilStateCreateInfo depth_stencil_info = {};
};

struct DisabledBlendState {
	DisabledBlendState() noexcept {
		color_blend_state.blendEnable = VK_FALSE;
		color_blend_state.colorWriteMask =
		      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		color_blend_state.colorBlendOp = VK_BLEND_OP_ADD;
		color_blend_state.alphaBlendOp = VK_BLEND_OP_ADD;
		color_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blend_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		color_blend_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	}

	VkPipelineColorBlendAttachmentState color_blend_state = {};
};

struct GraphicsPipelineParts {
	GraphicsPipelineParts() noexcept {
		auto &backend = VulkanBackend::backend();
		vxAssert(backend.shaderModuleCollection() != nullptr && backend.pipelineLayoutCollection() != nullptr);

		for (auto &info : create_infos) {
			info = {};
			info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			info.basePipelineIndex = -1;
		}
	}

	VkGraphicsPipelineCreateInfo create_infos[VulkanPipelineCollection::NUM_GRAPHICS_PIPELINES];

	const VertexFormatPositionOnly vert_fmt_pos_only;
	const DefaultViewportState default_viewport_state;
	const DefaultRasterizationState default_rasterization_state;
	const DisabledMsaaState disabled_msaa_state;
	const DefaultDepthStencilState default_depth_stencil_state;
	const DisabledBlendState disabled_blend_state;

	struct {
		VkPipelineShaderStageCreateInfo stages[2] = { {}, {} };
		VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {};
		VkPipelineRasterizationStateCreateInfo rasterization_info = {};
		VkPipelineColorBlendStateCreateInfo color_blend_info = {};
	} debug_octree_parts;
};

template<uint32_t ID>
void addParts(GraphicsPipelineParts &parts, VulkanBackend &backend);

template<>
void addParts<VulkanPipelineCollection::DEBUG_OCTREE_PIPELINE>(GraphicsPipelineParts &parts, VulkanBackend &backend) {
	auto &my_parts = parts.debug_octree_parts;
	auto &my_create_info = parts.create_infos[VulkanPipelineCollection::DEBUG_OCTREE_PIPELINE];

	auto &vert_stage_info = my_parts.stages[0];
	vert_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vert_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vert_stage_info.module = backend.shaderModuleCollection()->debugOctreeVertexShader();
	vert_stage_info.pName = "main";

	auto &frag_stage_info = my_parts.stages[1];
	frag_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	frag_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	frag_stage_info.module = backend.shaderModuleCollection()->debugOctreeFragmentShader();
	frag_stage_info.pName = "main";

	auto &input_assembly_info = my_parts.input_assembly_info;
	input_assembly_info.sType =  VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	input_assembly_info.primitiveRestartEnable = VK_FALSE;

	auto &rasterization_info = my_parts.rasterization_info;
	rasterization_info = parts.default_rasterization_state.rasterization_info;
	rasterization_info.polygonMode = VK_POLYGON_MODE_LINE;

	auto &color_blend_info = my_parts.color_blend_info;
	color_blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blend_info.logicOpEnable = VK_FALSE;
	color_blend_info.attachmentCount = 1;
	color_blend_info.pAttachments = &parts.disabled_blend_state.color_blend_state;

	my_create_info.stageCount = 2;
	my_create_info.pStages = my_parts.stages;
	my_create_info.pVertexInputState = &parts.vert_fmt_pos_only.vertex_input_info;
	my_create_info.pInputAssemblyState = &input_assembly_info;
	my_create_info.pViewportState = &parts.default_viewport_state.viewport_info;
	my_create_info.pRasterizationState = &rasterization_info;
	my_create_info.pMultisampleState = &parts.disabled_msaa_state.multisample_info;
	my_create_info.pDepthStencilState = &parts.default_depth_stencil_state.depth_stencil_info;
	my_create_info.pColorBlendState = &color_blend_info;
	my_create_info.layout = backend.pipelineLayoutCollection()->descriptorlessLayout();
	my_create_info.renderPass = backend.renderPassCollection()->mainRenderPass();
	my_create_info.subpass = 0;
}

template<uint32_t ID>
void fillPipelineParts(GraphicsPipelineParts &parts, VulkanBackend &backend) {
	if constexpr (ID < VulkanPipelineCollection::NUM_GRAPHICS_PIPELINES) {
		addParts<ID>(parts, backend);
		fillPipelineParts<ID + 1>(parts, backend);
	}
}

VulkanPipelineCollection::VulkanPipelineCollection() {
	Log::debug("Creating VulkanPipelineCollection");

	auto &backend = VulkanBackend::backend();
	vxAssert(backend.pipelineCache() != nullptr);
	VkDevice device = *backend.device();
	VkPipelineCache cache = *backend.pipelineCache();
	auto allocator = VulkanHostAllocator::callbacks();

	auto graphics_parts = std::make_unique<GraphicsPipelineParts>();
	fillPipelineParts<0>(*graphics_parts, backend);

	VkResult result = backend.vkCreateGraphicsPipelines(device, cache, NUM_GRAPHICS_PIPELINES,
	                                                    graphics_parts->create_infos, allocator, m_graphics_pipelines);
	if (result != VK_SUCCESS) {
		// Some pipelines could have been successfully created, destroy them
		for (VkPipeline pipe : m_graphics_pipelines)
			backend.vkDestroyPipeline(device, pipe, allocator);
		throw VulkanException(result);
	}

	Log::debug("VulkanPipelineCollection created successfully");
}

VulkanPipelineCollection::~VulkanPipelineCollection() noexcept {
	Log::debug("Destroying VulkanPipelineCollection");

	auto &backend = VulkanBackend::backend();
	VkDevice device = *backend.device();
	auto allocator = VulkanHostAllocator::callbacks();

	for (VkPipeline pipe : m_graphics_pipelines)
		backend.vkDestroyPipeline(device, pipe, allocator);
}

}
