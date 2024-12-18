#pragma once

#include <voxen/common/uid.hpp>

#include <vulkan/vulkan.h>

namespace voxen::gfx::vk
{

struct ShaderRequest {
	std::span<const uint32_t> code;
	std::span<const uint32_t> specialization;
};

struct ComputePipelineRequest {
	ShaderRequest shader;
	VkPipelineLayout layout = VK_NULL_HANDLE;
};

struct FixedFunctionState {
	VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
	VkPolygonMode polygon_mode = VK_POLYGON_MODE_MAX_ENUM;

	uint32_t num_color_attachments = 0;
	VkPipelineColorBlendAttachmentState blend_state[8] = {};
	VkFormat color_formats[8] = {};
	VkFormat depth_format = VK_FORMAT_UNDEFINED;
	VkFormat stencil_format = VK_FORMAT_UNDEFINED;
};

struct GraphicsPipelineRequest {
	ShaderRequest vertex_shader;
	ShaderRequest fragment_shader;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	FixedFunctionState ffs;
};

class PipelineFactory {
public:

	VkPipeline makeComputePipeline(const ComputePipelineRequest &req);
	VkPipeline makeGraphicsPipeline(const GraphicsPipelineRequest &req);

private:

};

}
