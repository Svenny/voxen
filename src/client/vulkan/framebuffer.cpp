#include <voxen/client/vulkan/framebuffer.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>
#include <voxen/client/vulkan/render_pass.hpp>
#include <voxen/client/vulkan/surface.hpp>
#include <voxen/client/vulkan/swapchain.hpp>

#include <voxen/util/log.hpp>

namespace voxen::client::vulkan
{

Framebuffer::Framebuffer(const VkFramebufferCreateInfo &info)
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device();
	VkResult result = backend.vkCreateFramebuffer(device, &info, VulkanHostAllocator::callbacks(), &m_framebuffer);
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkCreateFramebuffer");
}

Framebuffer::~Framebuffer() noexcept
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device();
	backend.vkDestroyFramebuffer(device, m_framebuffer, VulkanHostAllocator::callbacks());
}

FramebufferCollection::FramebufferCollection() :
	m_scene_depth_stencil_buffer(createSceneDepthStencilBuffer()),
	m_scene_depth_stencil_buffer_view(createSceneDepthStencilBufferView()),
	m_scene_framebuffer(createSceneFramebuffer())
{
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

Framebuffer FramebufferCollection::createSceneFramebuffer()
{
	Log::debug("Creating scene framebuffer");

	auto &backend = Backend::backend();
	auto &surface = backend.surface();
	auto &swapchain = backend.swapchain();

	VkExtent2D frame_size = swapchain.imageExtent();
	VkFormat color_buffer_format = surface.format().format;

	VkFramebufferAttachmentImageInfo attachment_infos[2] = { {}, {} };

	VkFramebufferAttachmentImageInfo &color_buffer_info = attachment_infos[0];
	color_buffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO;
	color_buffer_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	color_buffer_info.width = frame_size.width;
	color_buffer_info.height = frame_size.height;
	color_buffer_info.layerCount = 1;
	color_buffer_info.viewFormatCount = 1;
	color_buffer_info.pViewFormats = &color_buffer_format;

	VkFramebufferAttachmentImageInfo &depth_stencil_buffer_info = attachment_infos[1];
	depth_stencil_buffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO;
	depth_stencil_buffer_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	depth_stencil_buffer_info.width = frame_size.width;
	depth_stencil_buffer_info.height = frame_size.height;
	depth_stencil_buffer_info.layerCount = 1;
	depth_stencil_buffer_info.viewFormatCount = 1;
	depth_stencil_buffer_info.pViewFormats = &SCENE_DEPTH_STENCIL_BUFFER_FORMAT;

	VkFramebufferAttachmentsCreateInfo attachments_info = {};
	attachments_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO;
	attachments_info.attachmentImageInfoCount = 2;
	attachments_info.pAttachmentImageInfos = attachment_infos;

	VkFramebufferCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	info.pNext = &attachments_info;
	info.flags = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT;
	info.renderPass = backend.renderPassCollection().mainRenderPass();
	info.attachmentCount = 2;
	info.width = frame_size.width;
	info.height = frame_size.height;
	info.layers = 1;
	return Framebuffer(info);
}

}
