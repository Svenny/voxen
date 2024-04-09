#include <voxen/client/vulkan/framebuffer.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/config.hpp>
#include <voxen/client/vulkan/device.hpp>
#include <voxen/client/vulkan/surface.hpp>
#include <voxen/client/vulkan/swapchain.hpp>

#include <voxen/util/log.hpp>

namespace voxen::client::vulkan
{

FramebufferCollection::FramebufferCollection()
	: m_scene_depth_stencil_buffer(createSceneDepthStencilBuffer())
	, m_scene_depth_stencil_buffer_view(createSceneDepthStencilBufferView())
{
	// To allow multiple frames we need to keep N copies of render targets
	static_assert(Config::NUM_GPU_PENDING_FRAMES == 1, "A single pending GPU frame is currently assumed");
	Log::debug("FramebufferCollection created successfully");
}

Image FramebufferCollection::createSceneDepthStencilBuffer()
{
	Log::debug("Creating scene depth-stencil buffer");

	auto &backend = Backend::backend();
	assert(backend.swapchain() != nullptr);
	VkExtent2D extent = backend.swapchain().imageExtent();

	VkImageCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.imageType = VK_IMAGE_TYPE_2D;
	info.format = SCENE_DEPTH_STENCIL_BUFFER_FORMAT;
	info.extent = { extent.width, extent.height, 1 };
	info.mipLevels = 1;
	info.arrayLayers = 1;
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	return Image(info);
}

ImageView FramebufferCollection::createSceneDepthStencilBufferView()
{
	Log::debug("Creating scene depth-stencil buffer view");

	VkImageViewCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	info.image = m_scene_depth_stencil_buffer;
	info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	info.format = SCENE_DEPTH_STENCIL_BUFFER_FORMAT;
	info.components.r = VK_COMPONENT_SWIZZLE_R;
	info.components.g = VK_COMPONENT_SWIZZLE_G;
	info.components.b = VK_COMPONENT_SWIZZLE_B;
	info.components.a = VK_COMPONENT_SWIZZLE_A;
	info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	info.subresourceRange.baseMipLevel = 0;
	info.subresourceRange.levelCount = 1;
	info.subresourceRange.baseArrayLayer = 0;
	info.subresourceRange.layerCount = 1;
	return ImageView(info);
}

} // namespace voxen::client::vulkan
