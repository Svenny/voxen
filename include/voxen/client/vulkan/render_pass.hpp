#pragma once

#include <voxen/client/vulkan/common.hpp>

namespace voxen::client::vulkan
{

// TODO: check for physical device support (tiling/usage bits)
// TODO: factor it out to attachments collection?
inline constexpr VkFormat SCENE_DEPTH_STENCIL_BUFFER_FORMAT = VK_FORMAT_D24_UNORM_S8_UINT;

class RenderPass {
public:
	explicit RenderPass(const VkRenderPassCreateInfo &info);
	RenderPass(RenderPass &&) = delete;
	RenderPass(const RenderPass &) = delete;
	RenderPass &operator = (RenderPass &&) = delete;
	RenderPass &operator = (const RenderPass &) = delete;
	~RenderPass() noexcept;

	operator VkRenderPass() const noexcept { return m_render_pass; }
private:
	VkRenderPass m_render_pass = VK_NULL_HANDLE;
};

class RenderPassCollection {
public:
	RenderPassCollection();
	RenderPassCollection(RenderPassCollection &&) = delete;
	RenderPassCollection(const RenderPassCollection &) = delete;
	RenderPassCollection &operator = (RenderPassCollection &&) = delete;
	RenderPassCollection &operator = (const RenderPassCollection &) = delete;
	~RenderPassCollection() = default;

	RenderPass &mainRenderPass() noexcept { return m_main_render_pass; }
private:
	VkAttachmentDescription m_swapchain_color_buffer;
	VkAttachmentDescription m_scene_depth_stencil_buffer;
	RenderPass m_main_render_pass;

	VkAttachmentDescription describeSwapchainColorBuffer();
	VkAttachmentDescription describeSceneDepthStencilBuffer();
	RenderPass createMainRenderPass();
};

}
