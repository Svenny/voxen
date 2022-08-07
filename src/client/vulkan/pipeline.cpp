#include <voxen/client/vulkan/pipeline.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>
#include <voxen/client/vulkan/pipeline_cache.hpp>
#include <voxen/client/vulkan/pipeline_layout.hpp>
#include <voxen/client/vulkan/render_pass.hpp>
#include <voxen/client/vulkan/shader_module.hpp>

#include <voxen/util/log.hpp>

#include <extras/defer.hpp>

namespace voxen::client::vulkan
{

struct VertexFormatPositionOnlyTerrain {
	VertexFormatPositionOnlyTerrain() noexcept
	{
		// Per-vertex data
		vertex_input_binding[0] = VkVertexInputBindingDescription {
			.binding = 0,
			.stride = sizeof(float) * 3,
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
		};
		// Per-chunk data
		vertex_input_binding[1] = VkVertexInputBindingDescription {
			.binding = 1,
			.stride = sizeof(float) * 4,
			.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
		};

		// Position
		vertex_input_attrib[0] = VkVertexInputAttributeDescription {
			.location = 0,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = 0
		};
		// Chunk base/scale data
		vertex_input_attrib[1] = VkVertexInputAttributeDescription {
			.location = 1,
			.binding = 1,
			.format = VK_FORMAT_R32G32B32A32_SFLOAT,
			.offset = 0
		};

		vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertex_input_info.vertexBindingDescriptionCount = uint32_t(std::size(vertex_input_binding));
		vertex_input_info.pVertexBindingDescriptions = vertex_input_binding;
		vertex_input_info.vertexAttributeDescriptionCount = uint32_t(std::size(vertex_input_attrib));
		vertex_input_info.pVertexAttributeDescriptions = vertex_input_attrib;
	}

	VkVertexInputBindingDescription vertex_input_binding[2] = {};
	VkVertexInputAttributeDescription vertex_input_attrib[2] = {};
	VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
};

struct VertexFormatBasicTerrain {
	VertexFormatBasicTerrain() noexcept
	{
		// Per-vertex data
		vertex_input_binding[0] = VkVertexInputBindingDescription {
			.binding = 0,
			.stride = sizeof(float) * 6,
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
		};
		// Per-chunk data
		vertex_input_binding[1] = VkVertexInputBindingDescription {
			.binding = 1,
			.stride = sizeof(float) * 4,
			.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
		};

		// Position
		vertex_input_attrib[0] = VkVertexInputAttributeDescription {
			.location = 0,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = 0
		};
		// Normal
		vertex_input_attrib[1] = VkVertexInputAttributeDescription {
			.location = 1,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = sizeof(float) * 3
		};
		// Material ID
		vertex_input_attrib[2] = VkVertexInputAttributeDescription {
			.location = 2,
			.binding = 0,
			.format = VK_FORMAT_R32_SFLOAT,
			.offset = sizeof(float) * 6
		};
		// Chunk base/scale data
		vertex_input_attrib[3] = VkVertexInputAttributeDescription {
			.location = 3,
			.binding = 1,
			.format = VK_FORMAT_R32G32B32A32_SFLOAT,
			.offset = 0
		};

		vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertex_input_info.vertexBindingDescriptionCount = uint32_t(std::size(vertex_input_binding));
		vertex_input_info.pVertexBindingDescriptions = vertex_input_binding;
		vertex_input_info.vertexAttributeDescriptionCount = uint32_t(std::size(vertex_input_attrib));
		vertex_input_info.pVertexAttributeDescriptions = vertex_input_attrib;
	}

	VkVertexInputBindingDescription vertex_input_binding[2] = {};
	VkVertexInputAttributeDescription vertex_input_attrib[4] = {};
	VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
};

struct DefaultInputAssemblyState {
	DefaultInputAssemblyState() noexcept
	{
		input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		input_assembly_info.primitiveRestartEnable = VK_FALSE;
	}

	VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {};
};

struct DefaultViewportState {
	DefaultViewportState() noexcept
	{
		// Effectively disable scissoring at all, we rely on setting the viewport correctly
		scissor.offset = { 0, 0 };
		scissor.extent = { INT32_MAX, INT32_MAX };

		viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_info.scissorCount = 1;
		viewport_info.pScissors = &scissor;
		viewport_info.viewportCount = 1;
		viewport_info.pViewports = nullptr;

		dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic_state_info.dynamicStateCount = 1;
		dynamic_state_info.pDynamicStates = &viewport_dynamic_state;
	}

	VkRect2D scissor;
	VkPipelineViewportStateCreateInfo viewport_info = {};

	const VkDynamicState viewport_dynamic_state = VK_DYNAMIC_STATE_VIEWPORT;
	VkPipelineDynamicStateCreateInfo dynamic_state_info = {};
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

		color_blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		color_blend_info.logicOpEnable = VK_FALSE;
		color_blend_info.attachmentCount = 1;
		color_blend_info.pAttachments = &color_blend_state;
	}

	VkPipelineColorBlendAttachmentState color_blend_state = {};
	VkPipelineColorBlendStateCreateInfo color_blend_info = {};
};

struct GraphicsPipelineParts {
	GraphicsPipelineParts() noexcept
	{
		for (auto &info : create_infos) {
			info = {};
			info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			info.basePipelineIndex = -1;
		}
	}

	VkGraphicsPipelineCreateInfo create_infos[PipelineCollection::NUM_GRAPHICS_PIPELINES];

	const VertexFormatPositionOnlyTerrain vert_fmt_pos_only_terrain;
	const VertexFormatBasicTerrain vert_fmt_basic_terrain;
	const DefaultInputAssemblyState default_input_assembly_state;
	const DefaultViewportState default_viewport_state;
	const DefaultRasterizationState default_rasterization_state;
	const DisabledMsaaState disabled_msaa_state;
	const DefaultDepthStencilState default_depth_stencil_state;
	const DisabledBlendState disabled_blend_state;

	struct {
		VkPipelineShaderStageCreateInfo stages[2] = { {}, {} };
		VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {};
		VkPipelineRasterizationStateCreateInfo rasterization_info = {};
	} debug_octree_parts;

	struct {
		VkPipelineShaderStageCreateInfo stages[2] = { {}, {} };
	} terrain_simple_parts;
};

template<uint32_t ID>
void addParts(GraphicsPipelineParts &parts, Backend &backend);

template<>
void addParts<PipelineCollection::DEBUG_OCTREE_PIPELINE>(GraphicsPipelineParts &parts, Backend &backend)
{
	auto &my_parts = parts.debug_octree_parts;
	auto &my_create_info = parts.create_infos[PipelineCollection::DEBUG_OCTREE_PIPELINE];
	auto &module_collection = backend.shaderModuleCollection();

	auto &vert_stage_info = my_parts.stages[0];
	vert_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vert_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vert_stage_info.module = module_collection[ShaderModuleCollection::DEBUG_OCTREE_VERTEX];
	vert_stage_info.pName = "main";

	auto &frag_stage_info = my_parts.stages[1];
	frag_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	frag_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	frag_stage_info.module = module_collection[ShaderModuleCollection::DEBUG_OCTREE_FRAGMENT];
	frag_stage_info.pName = "main";

	auto &input_assembly_info = my_parts.input_assembly_info;
	input_assembly_info.sType =  VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	input_assembly_info.primitiveRestartEnable = VK_FALSE;

	auto &rasterization_info = my_parts.rasterization_info;
	rasterization_info = parts.default_rasterization_state.rasterization_info;
	rasterization_info.polygonMode = VK_POLYGON_MODE_FILL;

	my_create_info.stageCount = 2;
	my_create_info.pStages = my_parts.stages;
	my_create_info.pVertexInputState = &parts.vert_fmt_pos_only_terrain.vertex_input_info;
	my_create_info.pInputAssemblyState = &input_assembly_info;
	my_create_info.pViewportState = &parts.default_viewport_state.viewport_info;
	my_create_info.pRasterizationState = &rasterization_info;
	my_create_info.pMultisampleState = &parts.disabled_msaa_state.multisample_info;
	my_create_info.pDepthStencilState = &parts.default_depth_stencil_state.depth_stencil_info;
	my_create_info.pColorBlendState = &parts.disabled_blend_state.color_blend_info;
	my_create_info.pDynamicState = &parts.default_viewport_state.dynamic_state_info;
	my_create_info.layout = backend.pipelineLayoutCollection().terrainBasicLayout();
	my_create_info.renderPass = backend.renderPassCollection().mainRenderPass();
	my_create_info.subpass = 0;
}

template<>
void addParts<PipelineCollection::TERRAIN_SIMPLE_PIPELINE>(GraphicsPipelineParts &parts, Backend &backend)
{
	auto &my_parts = parts.terrain_simple_parts;
	auto &my_create_info = parts.create_infos[PipelineCollection::TERRAIN_SIMPLE_PIPELINE];
	auto &module_collection = backend.shaderModuleCollection();

	auto &vert_stage_info = my_parts.stages[0];
	vert_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vert_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vert_stage_info.module = module_collection[ShaderModuleCollection::TERRAIN_SIMPLE_VERTEX];
	vert_stage_info.pName = "main";

	auto &frag_stage_info = my_parts.stages[1];
	frag_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	frag_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	frag_stage_info.module = module_collection[ShaderModuleCollection::TERRAIN_SIMPLE_FRAGMENT];
	frag_stage_info.pName = "main";

	my_create_info.stageCount = 2;
	my_create_info.pStages = my_parts.stages;
	my_create_info.pVertexInputState = &parts.vert_fmt_basic_terrain.vertex_input_info;
	my_create_info.pInputAssemblyState = &parts.default_input_assembly_state.input_assembly_info;
	my_create_info.pViewportState = &parts.default_viewport_state.viewport_info;
	my_create_info.pRasterizationState = &parts.default_rasterization_state.rasterization_info;
	my_create_info.pMultisampleState = &parts.disabled_msaa_state.multisample_info;
	my_create_info.pDepthStencilState = &parts.default_depth_stencil_state.depth_stencil_info;
	my_create_info.pColorBlendState = &parts.disabled_blend_state.color_blend_info;
	my_create_info.pDynamicState = &parts.default_viewport_state.dynamic_state_info;
	my_create_info.layout = backend.pipelineLayoutCollection().terrainBasicLayout();
	my_create_info.renderPass = backend.renderPassCollection().mainRenderPass();
	my_create_info.subpass = 0;
}

struct ComputePipelineParts {
	ComputePipelineParts() noexcept
	{
		for (auto &info : create_infos) {
			info = {};
			info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			info.pNext = nullptr;
			info.flags = 0;
			info.basePipelineHandle = VK_NULL_HANDLE;
			info.basePipelineIndex = -1;
		}
	}

	VkComputePipelineCreateInfo create_infos[PipelineCollection::NUM_COMPUTE_PIPELINES];
};

template<uint32_t ID>
void addParts(ComputePipelineParts &parts, Backend &backend);

template<>
void addParts<PipelineCollection::TERRAIN_FRUSTUM_CULL_PIPELINE>(ComputePipelineParts &parts, Backend &backend)
{
	auto &my_create_info = parts.create_infos[PipelineCollection::TERRAIN_FRUSTUM_CULL_PIPELINE];
	auto &module_collection = backend.shaderModuleCollection();

	my_create_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	my_create_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	my_create_info.stage.module = module_collection[ShaderModuleCollection::TERRAIN_FRUSTUM_CULL_COMPUTE];
	my_create_info.stage.pName = "main";

	my_create_info.layout = backend.pipelineLayoutCollection().terrainFrustumCullLayout();
}

template<uint32_t ID = 0>
static void fillPipelineParts(GraphicsPipelineParts &parts, Backend &backend)
{
	if constexpr (ID < PipelineCollection::NUM_GRAPHICS_PIPELINES) {
		addParts<ID>(parts, backend);
		fillPipelineParts<ID + 1>(parts, backend);
	}
}

template<uint32_t ID = 0>
static void fillPipelineParts(ComputePipelineParts &parts, Backend &backend)
{
	if constexpr (ID < PipelineCollection::NUM_COMPUTE_PIPELINES) {
		addParts<ID>(parts, backend);
		fillPipelineParts<ID + 1>(parts, backend);
	}
}

PipelineCollection::PipelineCollection()
{
	m_graphics_pipelines.fill(VK_NULL_HANDLE);
	m_compute_pipelines.fill(VK_NULL_HANDLE);
	defer_fail { destroyPipelines(); };

	Log::debug("Creating PipelineCollection");

	auto &backend = Backend::backend();
	assert(backend.pipelineCache() != nullptr);
	VkDevice device = backend.device();
	VkPipelineCache cache = backend.pipelineCache();
	auto allocator = HostAllocator::callbacks();

	auto graphics_parts = std::make_unique<GraphicsPipelineParts>();
	fillPipelineParts(*graphics_parts, backend);

	VkResult result = backend.vkCreateGraphicsPipelines(device, cache, NUM_GRAPHICS_PIPELINES,
		graphics_parts->create_infos, allocator, m_graphics_pipelines.data());
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkCreateGraphicsPipelines");
	}

	auto compute_parts = std::make_unique<ComputePipelineParts>();
	fillPipelineParts(*compute_parts, backend);

	result = backend.vkCreateComputePipelines(device, cache, NUM_COMPUTE_PIPELINES,
		compute_parts->create_infos, allocator, m_compute_pipelines.data());
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkCreateComputePipelines");
	}

	Log::debug("PipelineCollection created successfully");
}

PipelineCollection::~PipelineCollection() noexcept
{
	Log::debug("Destroying PipelineCollection");
	destroyPipelines();
}

VkPipeline PipelineCollection::operator[](GraphicsPipelineId idx) const noexcept
{
	assert(idx < NUM_GRAPHICS_PIPELINES);
	return m_graphics_pipelines[idx];
}

VkPipeline PipelineCollection::operator[](ComputePipelineId idx) const noexcept
{
	assert(idx < NUM_COMPUTE_PIPELINES);
	return m_compute_pipelines[idx];
}

void PipelineCollection::destroyPipelines() noexcept
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device();
	auto allocator = HostAllocator::callbacks();

	for (VkPipeline &pipe : m_graphics_pipelines) {
		backend.vkDestroyPipeline(device, pipe, allocator);
		pipe = VK_NULL_HANDLE;
	}

	for (VkPipeline &pipe : m_compute_pipelines) {
		backend.vkDestroyPipeline(device, pipe, allocator);
		pipe = VK_NULL_HANDLE;
	}
}

}
