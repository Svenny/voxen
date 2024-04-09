#pragma once

#include <voxen/client/vulkan/common.hpp>
#include <voxen/client/vulkan/image.hpp>
#include <voxen/client/vulkan/image_view.hpp>

namespace voxen::client::vulkan
{

// TODO: check for physical device support (tiling/usage bits)
// TODO: factor it out to attachments collection?
// TODO: use extended depth range? (currently it's 0..1, effectively losing
// advantage of 32-bit buffer) or use D24 when supported?
inline constexpr VkFormat SCENE_DEPTH_STENCIL_BUFFER_FORMAT = VK_FORMAT_D32_SFLOAT;

class FramebufferCollection {
public:
	FramebufferCollection();
	FramebufferCollection(FramebufferCollection &&) = delete;
	FramebufferCollection(const FramebufferCollection &) = delete;
	FramebufferCollection &operator=(FramebufferCollection &&) = delete;
	FramebufferCollection &operator=(const FramebufferCollection &) = delete;
	~FramebufferCollection() = default;

	Image &sceneDepthStencilBuffer() noexcept { return m_scene_depth_stencil_buffer; }
	ImageView &sceneDepthStencilBufferView() noexcept { return m_scene_depth_stencil_buffer_view; }

private:
	Image m_scene_depth_stencil_buffer;
	ImageView m_scene_depth_stencil_buffer_view;

	Image createSceneDepthStencilBuffer();
	ImageView createSceneDepthStencilBufferView();
};

} // namespace voxen::client::vulkan
