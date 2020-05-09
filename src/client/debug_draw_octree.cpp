#include <voxen/client/debug_draw_octree.hpp>
#include <voxen/client/vulkan/common.hpp>
#include <voxen/util/log.hpp>

#include <extras/defer.hpp>

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>

namespace voxen
{

constexpr static VkDeviceSize k_vertex_size = 3 * 4;
constexpr static VkDeviceSize k_num_vertices = 8;
constexpr static VkDeviceSize k_index_size = 2;
constexpr static VkDeviceSize k_num_indices = 2 * 12;

struct PushConstantsBlock {
	float mtx[16];
	float color[4];
};

DebugDrawOctree::DebugDrawOctree(VkDevice dev, VkRenderPass render_pass, uint32_t w, uint32_t h)
   : m_dev(dev), m_vertex_shader(dev), m_fragment_shader(dev) {
	auto allocator = VulkanHostAllocator::callbacks();
	{
		VkBufferCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.size = k_vertex_size * k_num_vertices;
		info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VkResult result = vkCreateBuffer(dev, &info, allocator, &m_vertex_buffer);
		if (result != VK_SUCCESS) {
			Log::error("Failed to create vertex buffer");
			throw VulkanException(result);
		}
	}
	defer_fail { vkDestroyBuffer(m_dev, m_vertex_buffer, allocator); };
	{
		VkBufferCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.size = k_index_size * k_num_indices;
		info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VkResult result = vkCreateBuffer(dev, &info, allocator, &m_index_buffer);
		if (result != VK_SUCCESS) {
			Log::error("Failed to create index buffer");
			throw VulkanException(result);
		}
	}
	defer_fail { vkDestroyBuffer(m_dev, m_index_buffer, allocator); };
	VkMemoryRequirements vertex_buffer_reqs;
	vkGetBufferMemoryRequirements(dev, m_vertex_buffer, &vertex_buffer_reqs);
	VkMemoryRequirements index_buffer_reqs;
	vkGetBufferMemoryRequirements(dev, m_index_buffer, &index_buffer_reqs);
	VkDeviceSize index_buffer_offset = k_vertex_size * k_num_vertices;
	{
		// If needed, add some padding to reach the required alignment for index buffer
		VkDeviceSize rem = index_buffer_offset % index_buffer_reqs.alignment;
		if (rem != 0)
			index_buffer_offset += index_buffer_reqs.alignment - rem;
	}
	{
		//VkPhysicalDeviceMemoryProperties mem_props;
		// TODO: check memory requirements!
		// On Intel this will work because it exposes only one memory type suitable for everything

		VkMemoryAllocateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		info.allocationSize = index_buffer_offset + k_index_size * k_num_indices;
		info.memoryTypeIndex = (voxen::BuildConfig::kUseIntegratedGpu ? 0 : 10); // Use 10 on Nvidia
		VkResult result = vkAllocateMemory(dev, &info, allocator, &m_memory);
		if (result != VK_SUCCESS) {
			Log::error("Failed to allocate memory for buffers");
			throw VulkanException(result);
		}
	}
	defer_fail { vkFreeMemory(m_dev, m_memory, allocator); };
	{
		void *data;
		VkResult result = vkMapMemory(dev, m_memory, 0, VK_WHOLE_SIZE, 0, &data);
		if (result != VK_SUCCESS) {
			Log::error("Failed to map memory");
			throw VulkanException(result);
		}
		defer { vkUnmapMemory(m_dev, m_memory); };

		result = vkBindBufferMemory(dev, m_vertex_buffer, m_memory, 0);
		if (result != VK_SUCCESS) {
			Log::error("Failed to bind vertex buffer memory");
			throw VulkanException(result);
		}
		const float vb_data[] = {
		   0.0f, 0.0f, 0.0f,
		   0.0f, 0.0f, 1.0f,
		   0.0f, 1.0f, 0.0f,
		   0.0f, 1.0f, 1.0f,
		   1.0f, 0.0f, 0.0f,
		   1.0f, 0.0f, 1.0f,
		   1.0f, 1.0f, 0.0f,
		   1.0f, 1.0f, 1.0f
		};
		memcpy(data, vb_data, sizeof(vb_data));

		result = vkBindBufferMemory(m_dev, m_index_buffer, m_memory, index_buffer_offset);
		if (result != VK_SUCCESS) {
			Log::error("Failed to bind index buffer memory");
			throw VulkanException(result);
		}
		const uint16_t ib_data[] = {
		   0, 1, 1, 5, 4, 5, 0, 4,
		   2, 3, 3, 7, 6, 7, 2, 6,
		   0, 2, 1, 3, 4, 6, 5, 7
		};
		memcpy(reinterpret_cast<std::byte *>(data) + index_buffer_offset, ib_data, sizeof(ib_data));
	}
	{
		VkPushConstantRange push_constant_range = {};
		push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		push_constant_range.offset = 0;
		push_constant_range.size = sizeof(PushConstantsBlock);

		VkPipelineLayoutCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		info.setLayoutCount = 0;
		info.pSetLayouts = nullptr;
		info.pushConstantRangeCount = 1;
		info.pPushConstantRanges = &push_constant_range;
		VkResult result = vkCreatePipelineLayout(dev, &info, allocator, &m_pipeline_layout);
		if (result != VK_SUCCESS) {
			Log::error("Failed to create pipeline layout");
			throw VulkanException(result);
		}
	}
	defer_fail { vkDestroyPipelineLayout(m_dev, m_pipeline_layout, allocator); };
	{
		if (!m_vertex_shader.load("vert.spv")) {
			Log::error("Failed to load vertex shader");
			throw MessageException("DebugDrawOctree creation failed");
		}
		if (!m_fragment_shader.load("frag.spv")) {
			Log::error("Failed to load fragment shader");
			throw MessageException("DebugDrawOctree creation failed");
		}
		VkPipelineShaderStageCreateInfo shader_stage_info[2];
		shader_stage_info[0] = {};
		shader_stage_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shader_stage_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		shader_stage_info[0].module = m_vertex_shader;
		shader_stage_info[0].pName = "main";
		shader_stage_info[1] = {};
		shader_stage_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shader_stage_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		shader_stage_info[1].module = m_fragment_shader;
		shader_stage_info[1].pName = "main";

		// Vertex input binding description
		VkVertexInputBindingDescription vertex_input_binding_desc;
		vertex_input_binding_desc.binding = 0;
		vertex_input_binding_desc.stride = k_vertex_size;
		vertex_input_binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		// Vertex input attribute description
		VkVertexInputAttributeDescription vertex_input_attrib_desc;
		vertex_input_attrib_desc.binding = 0;
		vertex_input_attrib_desc.location = 0;
		vertex_input_attrib_desc.format = VK_FORMAT_R32G32B32_SFLOAT;
		vertex_input_attrib_desc.offset = 0;
		// Vertex input state
		VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
		vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertex_input_info.vertexBindingDescriptionCount = 1;
		vertex_input_info.pVertexBindingDescriptions = &vertex_input_binding_desc;
		vertex_input_info.vertexAttributeDescriptionCount = 1;
		vertex_input_info.pVertexAttributeDescriptions = &vertex_input_attrib_desc;

		// Input assembly state
		VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {};
		input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		input_assembly_info.primitiveRestartEnable = VK_FALSE;

		// Scissor & viewport
		VkRect2D scissor;
		scissor.offset = { 0, 0 };
		scissor.extent = { w, h };
		VkViewport viewport;
		viewport.x = viewport.y = 0.0f;
		viewport.width = float(w);
		viewport.height = float(h);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		// Viewport state
		VkPipelineViewportStateCreateInfo viewport_info = {};
		viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_info.scissorCount = 1;
		viewport_info.pScissors = &scissor;
		viewport_info.viewportCount = 1;
		viewport_info.pViewports = &viewport;

		// Rasterization state
		VkPipelineRasterizationStateCreateInfo rasterization_info = {};
		rasterization_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterization_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterization_info.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterization_info.polygonMode = VK_POLYGON_MODE_LINE;
		rasterization_info.lineWidth = 1.0f;
		rasterization_info.depthBiasEnable = VK_FALSE;
		rasterization_info.depthClampEnable = VK_FALSE;
		rasterization_info.rasterizerDiscardEnable = VK_FALSE;

		// Multisample state
		VkPipelineMultisampleStateCreateInfo multisample_info = {};
		multisample_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisample_info.sampleShadingEnable = VK_FALSE;
		multisample_info.minSampleShading = 1.0f;
		multisample_info.alphaToCoverageEnable = VK_FALSE;
		multisample_info.alphaToOneEnable = VK_FALSE;

		// Color blend attachment state
		VkPipelineColorBlendAttachmentState color_blend_attachment = {};
		color_blend_attachment.blendEnable = VK_FALSE;
		color_blend_attachment.colorWriteMask =
		      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
		color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
		color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		// Color blend state
		VkPipelineColorBlendStateCreateInfo color_blend_info = {};
		color_blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		color_blend_info.logicOpEnable = VK_FALSE;
		color_blend_info.attachmentCount = 1;
		color_blend_info.pAttachments = &color_blend_attachment;

		// Finally create the pipeline itself
		VkGraphicsPipelineCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		info.stageCount = 2;
		info.pStages = shader_stage_info;
		info.pVertexInputState = &vertex_input_info;
		info.pInputAssemblyState = &input_assembly_info;
		info.pTessellationState = nullptr;
		info.pViewportState = &viewport_info;
		info.pRasterizationState = &rasterization_info;
		info.pMultisampleState = &multisample_info;
		info.pDepthStencilState = nullptr;
		info.pColorBlendState = &color_blend_info;
		info.pDynamicState = nullptr;
		info.layout = m_pipeline_layout;
		info.renderPass = render_pass;
		info.subpass = 0;
		info.basePipelineHandle = VK_NULL_HANDLE;
		info.basePipelineIndex = -1;
		VkResult result = vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &info, allocator, &m_pipeline);
		if (result != VK_SUCCESS) {
			Log::error("Failed to create graphics pipeline");
			throw VulkanException(result);
		}
	}
}

DebugDrawOctree::~DebugDrawOctree() {
	auto allocator = VulkanHostAllocator::callbacks();
	vkDestroyPipeline(m_dev, m_pipeline, allocator);
	vkDestroyPipelineLayout(m_dev, m_pipeline_layout, allocator);
	vkDestroyBuffer(m_dev, m_vertex_buffer, allocator);
	vkDestroyBuffer(m_dev, m_index_buffer, allocator);
	vkFreeMemory(m_dev, m_memory, allocator);
}

void DebugDrawOctree::beginRendering(VkCommandBuffer cmd_buf) {
	vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(cmd_buf, 0, 1, &m_vertex_buffer, &offset);
	// TODO: VK_INDEX_TYPE_UINT8_EXT?
	vkCmdBindIndexBuffer(cmd_buf, m_index_buffer, 0, VK_INDEX_TYPE_UINT16);
}

static constexpr float k_node_colors[6][4] = {
   { 1.0f, 0.0f, 0.0f, 1.0f },
   { 0.8f, 0.2f, 0.0f, 1.0f },
   { 0.6f, 0.4f, 0.0f, 1.0f },
   { 0.4f, 0.6f, 0.0f, 1.0f },
   { 0.2f, 0.8f, 0.0f, 1.0f },
   { 0.0f, 1.0f, 0.0f, 1.0f }
};

void DebugDrawOctree::drawNode(VkCommandBuffer cmd_buf, const glm::mat4 &view_proj_mat,
                               float base_x, float base_y, float base_z, float size) {
	glm::mat4 model_mat = extras::scale_translate(base_x, base_y, base_z, size);
	glm::mat4 mat = view_proj_mat * model_mat;
	PushConstantsBlock block;
	memcpy(block.mtx, glm::value_ptr(mat), sizeof(float) * 16);
	int idx = size == 32.0f ? 0 : 5;
	block.color[0] = k_node_colors[idx][0];
	block.color[1] = k_node_colors[idx][1];
	block.color[2] = k_node_colors[idx][2];
	block.color[3] = k_node_colors[idx][3];
	vkCmdPushConstants(cmd_buf, m_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
	                   0, sizeof(block), &block);
	vkCmdDrawIndexed(cmd_buf, k_num_indices, 1, 0, 0, 0);
}

void DebugDrawOctree::endRendering(VkCommandBuffer cmd_buf) {
	(void)cmd_buf;
}

}
