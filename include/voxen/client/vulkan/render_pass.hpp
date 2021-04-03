#pragma once

#include <voxen/client/vulkan/common.hpp>

namespace voxen::client::vulkan
{

// TODO: check for physical device support (tiling/usage bits)
// TODO: factor it out to attachments collection?
// TODO: use extended depth range? (currently it's 0..1, effectively losing
// advantage of 32-bit buffer) or use D24 when supported?
inline constexpr VkFormat SCENE_DEPTH_STENCIL_BUFFER_FORMAT = VK_FORMAT_D32_SFLOAT;

class RenderPass {
public:
	RenderPass() = default;
	explicit RenderPass(const VkRenderPassCreateInfo &info);
	explicit RenderPass(const VkRenderPassCreateInfo2 &info);
	RenderPass(RenderPass &&) noexcept;
	RenderPass(const RenderPass &) = delete;
	RenderPass &operator = (RenderPass &&) noexcept;
	RenderPass &operator = (const RenderPass &) = delete;
	~RenderPass() noexcept;

	operator VkRenderPass() const noexcept { return m_handle; }
private:
	VkRenderPass m_handle = VK_NULL_HANDLE;
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
