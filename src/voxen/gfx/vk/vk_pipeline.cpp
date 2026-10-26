#include <voxen/gfx/vk/vk_pipeline.hpp>

#include <voxen/common/filemanager.hpp>
#include <voxen/gfx/vk/legacy_render_graph.hpp>
#include <voxen/gfx/vk/vk_device.hpp>
#include <voxen/gfx/vk/vk_error.hpp>
#include <voxen/gfx/vk/vk_pipeline_cache.hpp>
#include <voxen/gfx/vk/vk_pipeline_layout.hpp>
#include <voxen/gfx/vk/vk_utils.hpp>
#include <voxen/log.hpp>

#include <extras/defer.hpp>

#include <array>
#include <cassert>
#include <iterator>
#include <memory>
#include <string_view>

namespace voxen::gfx::vk
{

namespace
{

// Shader modules are needed only during pipeline creation, they are destroyed right after it
class ShaderModuleCollection {
public:
	enum ShaderId : uint32_t {
		LAND_DEBUG_CHUNK_BOUNDS_VERTEX,
		LAND_DEBUG_CHUNK_BOUNDS_FRAGMENT,
		LAND_FRUSTUM_CULL_COMPUTE,
		LAND_CHUNK_MESH_VERTEX,
		LAND_CHUNK_MESH_FRAGMENT,
		LAND_SELECTOR_VERTEX,
		LAND_SELECTOR_FRAGMENT,
		UI_FONT_VERTEX,
		UI_FONT_FRAGMENT,
		UI_BASIC_VERTEX,
		UI_BASIC_FRAGMENT,

		NUM_SHADERS
	};

	explicit ShaderModuleCollection(Device &dev) : m_dev(dev)
	{
		defer_fail { destroyModules(); };

		for (uint32_t i = 0; i < NUM_SHADERS; i++) {
			m_shader_modules[i] = loadModule(SHADER_PATHS[i]);
		}
	}

	ShaderModuleCollection(ShaderModuleCollection &&) = delete;
	ShaderModuleCollection(const ShaderModuleCollection &) = delete;
	ShaderModuleCollection &operator=(ShaderModuleCollection &&) = delete;
	ShaderModuleCollection &operator=(const ShaderModuleCollection &) = delete;
	~ShaderModuleCollection() noexcept { destroyModules(); }

	VkShaderModule operator[](ShaderId idx) const noexcept { return m_shader_modules[idx]; }

private:
	// Indexed by `ShaderId`
	constexpr static std::string_view SHADER_PATHS[] = {
		"assets/shaders/land/debug_chunk_bounds.vert.spv",
		"assets/shaders/land/debug_chunk_bounds.frag.spv",
		"assets/shaders/land/frustum_cull.comp.spv",
		"assets/shaders/land/chunk_mesh.vert.spv",
		"assets/shaders/land/chunk_mesh.frag.spv",
		"assets/shaders/land/selector.vert.spv",
		"assets/shaders/land/selector.frag.spv",
		"assets/shaders/ui/font.vert.spv",
		"assets/shaders/ui/font.frag.spv",
		"assets/shaders/ui/ui_basic.vert.spv",
		"assets/shaders/ui/ui_basic.frag.spv",
	};
	static_assert(std::size(SHADER_PATHS) == NUM_SHADERS, "Add a path for every shader ID");

	Device &m_dev;
	std::array<VkShaderModule, NUM_SHADERS> m_shader_modules = {};

	VkShaderModule loadModule(std::string_view relative_path)
	{
		Log::debug("Loading shader module `{}`", relative_path);
		auto code = FileManager::readFile(relative_path);
		auto &code_bytes = code.value();

		VkShaderModuleCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		info.codeSize = code_bytes.size();
		info.pCode = reinterpret_cast<const uint32_t *>(code_bytes.data());

		VkShaderModule handle = VK_NULL_HANDLE;
		VkResult result = m_dev.dt().vkCreateShaderModule(m_dev.handle(), &info, nullptr, &handle);
		if (result != VK_SUCCESS) {
			throw VulkanException(result, "vkCreateShaderModule");
		}

		return handle;
	}

	void destroyModules() noexcept
	{
		for (VkShaderModule module : m_shader_modules) {
			m_dev.dt().vkDestroyShaderModule(m_dev.handle(), module, nullptr);
		}
	}
};

struct VertexFormatEmpty {
	VertexFormatEmpty() noexcept
	{
		vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertex_input_info.vertexBindingDescriptionCount = 0;
		vertex_input_info.pVertexBindingDescriptions = nullptr;
		vertex_input_info.vertexAttributeDescriptionCount = 0;
		vertex_input_info.pVertexAttributeDescriptions = nullptr;
	}

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
	DefaultRasterizationState() noexcept
	{
		rasterization_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		// Right-handed spaces with Y flipped by the projection, see `m/transform.hpp`
		rasterization_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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
	DisabledMsaaState() noexcept
	{
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
	DefaultDepthStencilState() noexcept
	{
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

struct DisabledDepthStencilState {
	DisabledDepthStencilState() noexcept
	{
		depth_stencil_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depth_stencil_info.depthTestEnable = VK_FALSE;
		depth_stencil_info.depthWriteEnable = VK_FALSE;
		depth_stencil_info.depthCompareOp = VK_COMPARE_OP_ALWAYS;
		depth_stencil_info.depthBoundsTestEnable = VK_FALSE;
		depth_stencil_info.stencilTestEnable = VK_FALSE;
		depth_stencil_info.minDepthBounds = 0.0f;
		depth_stencil_info.maxDepthBounds = 1.0f;
	}

	VkPipelineDepthStencilStateCreateInfo depth_stencil_info = {};
};

struct DisabledBlendState {
	DisabledBlendState() noexcept
	{
		color_blend_state.blendEnable = VK_FALSE;
		color_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
			| VK_COLOR_COMPONENT_A_BIT;
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

struct StandardBlendState {
	StandardBlendState() noexcept
	{
		color_blend_state.blendEnable = VK_TRUE;
		color_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
			| VK_COLOR_COMPONENT_A_BIT;
		color_blend_state.colorBlendOp = VK_BLEND_OP_ADD;
		color_blend_state.alphaBlendOp = VK_BLEND_OP_ADD;
		color_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		color_blend_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
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
	explicit GraphicsPipelineParts(VkFormat color_format) noexcept : default_surface_format(color_format)
	{
		for (auto &info : create_infos) {
			info = {};
			info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			info.basePipelineIndex = -1;
		}
	}

	VkGraphicsPipelineCreateInfo create_infos[PipelineCollection::NUM_GRAPHICS_PIPELINES];

	const VertexFormatEmpty vert_fmt_empty;
	const DefaultInputAssemblyState default_input_assembly_state;
	const DefaultViewportState default_viewport_state;
	const DefaultRasterizationState default_rasterization_state;
	const DisabledMsaaState disabled_msaa_state;
	const DefaultDepthStencilState default_depth_stencil_state;
	const DisabledDepthStencilState disabled_depth_stencil_state;
	const DisabledBlendState disabled_blend_state;
	const StandardBlendState standard_blend_state;

	VkFormat default_surface_format = VK_FORMAT_UNDEFINED;
	const VkFormat default_depth_format = LegacyRenderGraph::DEPTH_BUFFER_FORMAT;

	const VkPipelineRenderingCreateInfo default_rendering_info {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.pNext = nullptr,
		.viewMask = 0,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &default_surface_format,
		.depthAttachmentFormat = default_depth_format,
		.stencilAttachmentFormat = VulkanUtils::hasStencilComponent(default_depth_format)
			? default_depth_format
			: VK_FORMAT_UNDEFINED,
	};

	struct {
		VkPipelineShaderStageCreateInfo stages[2] = { {}, {} };
		VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {};
	} land_debug_chunk_bounds_parts;

	struct {
		VkPipelineShaderStageCreateInfo stages[2] = { {}, {} };
	} land_chunk_mesh_parts;

	struct {
		VkPipelineShaderStageCreateInfo stages[2] = { {}, {} };
		VkPipelineDepthStencilStateCreateInfo depth_stencil_info = {};
	} land_selector_parts;

	struct {
		VkPipelineShaderStageCreateInfo stages[2] = { {}, {} };
	} ui_font_parts;

	struct {
		VkPipelineShaderStageCreateInfo stages[2] = { {}, {} };
	} ui_basic_parts;
};

template<uint32_t ID>
void addParts(GraphicsPipelineParts &parts, const ShaderModuleCollection &module_collection,
	const PipelineLayoutCollection &layouts);

template<>
void addParts<PipelineCollection::LAND_DEBUG_CHUNK_BOUNDS_PIPELINE>(GraphicsPipelineParts &parts,
	const ShaderModuleCollection &module_collection, const PipelineLayoutCollection &layouts)
{
	auto &my_parts = parts.land_debug_chunk_bounds_parts;
	auto &my_create_info = parts.create_infos[PipelineCollection::LAND_DEBUG_CHUNK_BOUNDS_PIPELINE];

	auto &vert_stage_info = my_parts.stages[0];
	vert_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vert_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vert_stage_info.module = module_collection[ShaderModuleCollection::LAND_DEBUG_CHUNK_BOUNDS_VERTEX];
	vert_stage_info.pName = "main";

	auto &frag_stage_info = my_parts.stages[1];
	frag_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	frag_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	frag_stage_info.module = module_collection[ShaderModuleCollection::LAND_DEBUG_CHUNK_BOUNDS_FRAGMENT];
	frag_stage_info.pName = "main";

	auto &input_assembly_info = my_parts.input_assembly_info;
	input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	input_assembly_info.primitiveRestartEnable = VK_FALSE;

	my_create_info.stageCount = 2;
	my_create_info.pStages = my_parts.stages;
	my_create_info.pVertexInputState = &parts.vert_fmt_empty.vertex_input_info;
	my_create_info.pInputAssemblyState = &input_assembly_info;
	my_create_info.pViewportState = &parts.default_viewport_state.viewport_info;
	my_create_info.pRasterizationState = &parts.default_rasterization_state.rasterization_info;
	my_create_info.pMultisampleState = &parts.disabled_msaa_state.multisample_info;
	my_create_info.pDepthStencilState = &parts.default_depth_stencil_state.depth_stencil_info;
	my_create_info.pColorBlendState = &parts.disabled_blend_state.color_blend_info;
	my_create_info.pDynamicState = &parts.default_viewport_state.dynamic_state_info;
	my_create_info.layout = layouts.landChunkMeshLayout();
	my_create_info.renderPass = VK_NULL_HANDLE;
	my_create_info.subpass = 0;
	my_create_info.pNext = &parts.default_rendering_info;
}

template<>
void addParts<PipelineCollection::LAND_CHUNK_MESH_PIPELINE>(GraphicsPipelineParts &parts,
	const ShaderModuleCollection &module_collection, const PipelineLayoutCollection &layouts)
{
	auto &my_parts = parts.land_chunk_mesh_parts;

	auto &my_create_info = parts.create_infos[PipelineCollection::LAND_CHUNK_MESH_PIPELINE];

	auto &vert_stage_info = my_parts.stages[0];
	vert_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vert_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vert_stage_info.module = module_collection[ShaderModuleCollection::LAND_CHUNK_MESH_VERTEX];
	vert_stage_info.pName = "main";

	auto &frag_stage_info = my_parts.stages[1];
	frag_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	frag_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	frag_stage_info.module = module_collection[ShaderModuleCollection::LAND_CHUNK_MESH_FRAGMENT];
	frag_stage_info.pName = "main";

	my_create_info.stageCount = 2;
	my_create_info.pStages = my_parts.stages;
	my_create_info.pVertexInputState = &parts.vert_fmt_empty.vertex_input_info;
	my_create_info.pInputAssemblyState = &parts.default_input_assembly_state.input_assembly_info;
	my_create_info.pViewportState = &parts.default_viewport_state.viewport_info;
	my_create_info.pRasterizationState = &parts.default_rasterization_state.rasterization_info;
	my_create_info.pMultisampleState = &parts.disabled_msaa_state.multisample_info;
	my_create_info.pDepthStencilState = &parts.default_depth_stencil_state.depth_stencil_info;
	my_create_info.pColorBlendState = &parts.disabled_blend_state.color_blend_info;
	my_create_info.pDynamicState = &parts.default_viewport_state.dynamic_state_info;
	my_create_info.layout = layouts.landChunkMeshLayout();
	my_create_info.renderPass = VK_NULL_HANDLE;
	my_create_info.subpass = 0;
	my_create_info.pNext = &parts.default_rendering_info;
}

template<>
void addParts<PipelineCollection::LAND_SELECTOR_PIPELINE>(GraphicsPipelineParts &parts,
	const ShaderModuleCollection &module_collection, const PipelineLayoutCollection &layouts)
{
	auto &my_parts = parts.land_selector_parts;

	auto &my_create_info = parts.create_infos[PipelineCollection::LAND_SELECTOR_PIPELINE];

	auto &vert_stage_info = my_parts.stages[0];
	vert_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vert_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vert_stage_info.module = module_collection[ShaderModuleCollection::LAND_SELECTOR_VERTEX];
	vert_stage_info.pName = "main";

	auto &frag_stage_info = my_parts.stages[1];
	frag_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	frag_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	frag_stage_info.module = module_collection[ShaderModuleCollection::LAND_SELECTOR_FRAGMENT];
	frag_stage_info.pName = "main";

	auto &zs_info = my_parts.depth_stencil_info;
	zs_info = parts.default_depth_stencil_state.depth_stencil_info;
	zs_info.depthWriteEnable = VK_FALSE;

	my_create_info.stageCount = 2;
	my_create_info.pStages = my_parts.stages;
	my_create_info.pVertexInputState = &parts.vert_fmt_empty.vertex_input_info;
	my_create_info.pInputAssemblyState = &parts.default_input_assembly_state.input_assembly_info;
	my_create_info.pViewportState = &parts.default_viewport_state.viewport_info;
	my_create_info.pRasterizationState = &parts.default_rasterization_state.rasterization_info;
	my_create_info.pMultisampleState = &parts.disabled_msaa_state.multisample_info;
	my_create_info.pDepthStencilState = &zs_info;
	my_create_info.pColorBlendState = &parts.standard_blend_state.color_blend_info;
	my_create_info.pDynamicState = &parts.default_viewport_state.dynamic_state_info;
	my_create_info.layout = layouts.landSelectorLayout();
	my_create_info.renderPass = VK_NULL_HANDLE;
	my_create_info.subpass = 0;
	my_create_info.pNext = &parts.default_rendering_info;
}

template<>
void addParts<PipelineCollection::UI_FONT_PIPELINE>(GraphicsPipelineParts &parts,
	const ShaderModuleCollection &module_collection, const PipelineLayoutCollection &layouts)
{
	auto &my_parts = parts.ui_font_parts;
	auto &my_create_info = parts.create_infos[PipelineCollection::UI_FONT_PIPELINE];

	auto &vert_stage_info = my_parts.stages[0];
	vert_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vert_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vert_stage_info.module = module_collection[ShaderModuleCollection::UI_FONT_VERTEX];
	vert_stage_info.pName = "main";

	auto &frag_stage_info = my_parts.stages[1];
	frag_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	frag_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	frag_stage_info.module = module_collection[ShaderModuleCollection::UI_FONT_FRAGMENT];
	frag_stage_info.pName = "main";

	my_create_info.stageCount = 2;
	my_create_info.pStages = my_parts.stages;
	my_create_info.pVertexInputState = &parts.vert_fmt_empty.vertex_input_info;
	my_create_info.pInputAssemblyState = &parts.default_input_assembly_state.input_assembly_info;
	my_create_info.pViewportState = &parts.default_viewport_state.viewport_info;
	my_create_info.pRasterizationState = &parts.default_rasterization_state.rasterization_info;
	my_create_info.pMultisampleState = &parts.disabled_msaa_state.multisample_info;
	my_create_info.pDepthStencilState = &parts.disabled_depth_stencil_state.depth_stencil_info;
	my_create_info.pColorBlendState = &parts.standard_blend_state.color_blend_info;
	my_create_info.pDynamicState = &parts.default_viewport_state.dynamic_state_info;
	my_create_info.layout = layouts.uiFontLayout();
	my_create_info.renderPass = VK_NULL_HANDLE;
	my_create_info.subpass = 0;
	my_create_info.pNext = &parts.default_rendering_info;
}

template<>
void addParts<PipelineCollection::UI_BASIC_PIPELINE>(GraphicsPipelineParts &parts,
	const ShaderModuleCollection &module_collection, const PipelineLayoutCollection &layouts)
{
	auto &my_parts = parts.ui_basic_parts;
	auto &my_create_info = parts.create_infos[PipelineCollection::UI_BASIC_PIPELINE];

	auto &vert_stage_info = my_parts.stages[0];
	vert_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vert_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vert_stage_info.module = module_collection[ShaderModuleCollection::UI_BASIC_VERTEX];
	vert_stage_info.pName = "main";

	auto &frag_stage_info = my_parts.stages[1];
	frag_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	frag_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	frag_stage_info.module = module_collection[ShaderModuleCollection::UI_BASIC_FRAGMENT];
	frag_stage_info.pName = "main";

	my_create_info.stageCount = 2;
	my_create_info.pStages = my_parts.stages;
	my_create_info.pVertexInputState = &parts.vert_fmt_empty.vertex_input_info;
	my_create_info.pInputAssemblyState = &parts.default_input_assembly_state.input_assembly_info;
	my_create_info.pViewportState = &parts.default_viewport_state.viewport_info;
	my_create_info.pRasterizationState = &parts.default_rasterization_state.rasterization_info;
	my_create_info.pMultisampleState = &parts.disabled_msaa_state.multisample_info;
	my_create_info.pDepthStencilState = &parts.disabled_depth_stencil_state.depth_stencil_info;
	my_create_info.pColorBlendState = &parts.standard_blend_state.color_blend_info;
	my_create_info.pDynamicState = &parts.default_viewport_state.dynamic_state_info;
	my_create_info.layout = layouts.uiBasicLayout();
	my_create_info.renderPass = VK_NULL_HANDLE;
	my_create_info.subpass = 0;
	my_create_info.pNext = &parts.default_rendering_info;
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
void addParts(ComputePipelineParts &parts, const ShaderModuleCollection &module_collection,
	const PipelineLayoutCollection &layouts);

template<>
void addParts<PipelineCollection::LAND_FRUSTUM_CULL_PIPELINE>(ComputePipelineParts &parts,
	const ShaderModuleCollection &module_collection, const PipelineLayoutCollection &layouts)
{
	auto &my_create_info = parts.create_infos[PipelineCollection::LAND_FRUSTUM_CULL_PIPELINE];

	my_create_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	my_create_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	my_create_info.stage.module = module_collection[ShaderModuleCollection::LAND_FRUSTUM_CULL_COMPUTE];
	my_create_info.stage.pName = "main";

	my_create_info.layout = layouts.landFrustumCullLayout();
}

template<uint32_t ID = 0>
void fillPipelineParts(GraphicsPipelineParts &parts, const ShaderModuleCollection &module_collection,
	const PipelineLayoutCollection &layouts)
{
	if constexpr (ID < PipelineCollection::NUM_GRAPHICS_PIPELINES) {
		addParts<ID>(parts, module_collection, layouts);
		fillPipelineParts<ID + 1>(parts, module_collection, layouts);
	}
}

template<uint32_t ID = 0>
void fillPipelineParts(ComputePipelineParts &parts, const ShaderModuleCollection &module_collection,
	const PipelineLayoutCollection &layouts)
{
	if constexpr (ID < PipelineCollection::NUM_COMPUTE_PIPELINES) {
		addParts<ID>(parts, module_collection, layouts);
		fillPipelineParts<ID + 1>(parts, module_collection, layouts);
	}
}

} // namespace

PipelineCollection::PipelineCollection(Device &dev, const PipelineCache &cache, const PipelineLayoutCollection &layouts,
	VkFormat color_format)
	: m_dev(dev)
{
	m_graphics_pipelines.fill(VK_NULL_HANDLE);
	m_compute_pipelines.fill(VK_NULL_HANDLE);
	defer_fail { destroyPipelines(); };

	Log::debug("Creating PipelineCollection");

	VkDevice device = dev.handle();
	const ShaderModuleCollection module_collection(dev);

	auto graphics_parts = std::make_unique<GraphicsPipelineParts>(color_format);
	fillPipelineParts(*graphics_parts, module_collection, layouts);

	VkResult result = dev.dt().vkCreateGraphicsPipelines(device, cache.handle(), NUM_GRAPHICS_PIPELINES,
		graphics_parts->create_infos, nullptr, m_graphics_pipelines.data());
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkCreateGraphicsPipelines");
	}

	auto compute_parts = std::make_unique<ComputePipelineParts>();
	fillPipelineParts(*compute_parts, module_collection, layouts);

	result = dev.dt().vkCreateComputePipelines(device, cache.handle(), NUM_COMPUTE_PIPELINES,
		compute_parts->create_infos, nullptr, m_compute_pipelines.data());
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
	VkDevice device = m_dev.handle();

	for (VkPipeline &pipe : m_graphics_pipelines) {
		m_dev.dt().vkDestroyPipeline(device, pipe, nullptr);
		pipe = VK_NULL_HANDLE;
	}

	for (VkPipeline &pipe : m_compute_pipelines) {
		m_dev.dt().vkDestroyPipeline(device, pipe, nullptr);
		pipe = VK_NULL_HANDLE;
	}
}

} // namespace voxen::gfx::vk
