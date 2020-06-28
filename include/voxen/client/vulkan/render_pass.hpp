#pragma once

#include <voxen/client/vulkan/common.hpp>

namespace voxen::client
{

// TODO: check for physical device support (tiling/usage bits)
// TODO: factor it out to attachments collection?
inline constexpr VkFormat SCENE_DEPTH_STENCIL_BUFFER_FORMAT = VK_FORMAT_D24_UNORM_S8_UINT;

class VulkanRenderPass {
public:
	explicit VulkanRenderPass(const VkRenderPassCreateInfo &info);
	VulkanRenderPass(VulkanRenderPass &&) = delete;
	VulkanRenderPass(const VulkanRenderPass &) = delete;
	VulkanRenderPass &operator = (VulkanRenderPass &&) = delete;
	VulkanRenderPass &operator = (const VulkanRenderPass &) = delete;
	~VulkanRenderPass() noexcept;

	operator VkRenderPass() const noexcept { return m_render_pass; }
private:
	VkRenderPass m_render_pass = VK_NULL_HANDLE;
};

class VulkanRenderPassCollection {
public:
	VulkanRenderPassCollection();
	VulkanRenderPassCollection(VulkanRenderPassCollection &&) = delete;
	VulkanRenderPassCollection(const VulkanRenderPassCollection &) = delete;
	VulkanRenderPassCollection &operator = (VulkanRenderPassCollection &&) = delete;
	VulkanRenderPassCollection &operator = (const VulkanRenderPassCollection &) = delete;
	~VulkanRenderPassCollection() = default;

	VulkanRenderPass &mainRenderPass() noexcept { return m_main_render_pass; }
private:
	VkAttachmentDescription m_swapchain_color_buffer;
	VkAttachmentDescription m_scene_depth_stencil_buffer;
	VulkanRenderPass m_main_render_pass;

	VkAttachmentDescription describeSwapchainColorBuffer();
	VkAttachmentDescription describeSceneDepthStencilBuffer();
	VulkanRenderPass createMainRenderPass();
};

}
