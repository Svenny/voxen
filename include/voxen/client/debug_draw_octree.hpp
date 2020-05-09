#pragma once

#include <extras/math.hpp>

#include <voxen/client/vulkan/shader.hpp>

#include <vulkan/vulkan.h>

namespace voxen
{

class DebugDrawOctree {
public:
	DebugDrawOctree(VkDevice dev, VkRenderPass render_pass, uint32_t w, uint32_t h);
	~DebugDrawOctree();

	void beginRendering(VkCommandBuffer cmd_buf);
	void drawNode(VkCommandBuffer cmd_buf, const glm::mat4 &mat, float base_x, float base_y, float base_z, float size);
	void endRendering(VkCommandBuffer cmd_buf);

	VkPipeline pipelineHandle() const noexcept { return m_pipeline; }
private:
	const VkDevice m_dev = VK_NULL_HANDLE;

	VkDeviceMemory m_memory = VK_NULL_HANDLE;
	VkBuffer m_vertex_buffer = VK_NULL_HANDLE;
	VkBuffer m_index_buffer = VK_NULL_HANDLE;
	VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
	VkPipeline m_pipeline = VK_NULL_HANDLE;

	VulkanShader m_vertex_shader, m_fragment_shader;
};

}
