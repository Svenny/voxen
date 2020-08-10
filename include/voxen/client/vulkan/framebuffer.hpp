#pragma once

#include <voxen/client/vulkan/common.hpp>
#include <voxen/client/vulkan/image.hpp>
#include <voxen/client/vulkan/image_view.hpp>

namespace voxen::client::vulkan
{

class Framebuffer {
public:
	explicit Framebuffer(const VkFramebufferCreateInfo &info);
	Framebuffer(Framebuffer &&) = delete;
	Framebuffer(const Framebuffer &) = delete;
	Framebuffer &operator = (Framebuffer &&) = delete;
	Framebuffer &operator = (const Framebuffer &) = delete;
	~Framebuffer() noexcept;

	operator VkFramebuffer() const noexcept { return m_framebuffer; }
private:
	VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
};

class FramebufferCollection {
public:
	FramebufferCollection();
	FramebufferCollection(FramebufferCollection &&) = delete;
	FramebufferCollection(const FramebufferCollection &) = delete;
	FramebufferCollection &operator = (FramebufferCollection &&) = delete;
	FramebufferCollection &operator = (const FramebufferCollection &) = delete;
	~FramebufferCollection() = default;

	Image &sceneDepthStencilBuffer() noexcept { return m_scene_depth_stencil_buffer; }
	ImageView &sceneDepthStencilBufferView() noexcept { return m_scene_depth_stencil_buffer_view; }

	Framebuffer &sceneFramebuffer() noexcept { return m_scene_framebuffer; }
private:
	Image m_scene_depth_stencil_buffer;
	ImageView m_scene_depth_stencil_buffer_view;

	Framebuffer m_scene_framebuffer;

	Image createSceneDepthStencilBuffer();
	ImageView createSceneDepthStencilBufferView();
	Framebuffer createSceneFramebuffer();
};

}
