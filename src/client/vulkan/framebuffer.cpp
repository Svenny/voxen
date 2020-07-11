#include <voxen/client/vulkan/framebuffer.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>
#include <voxen/client/vulkan/render_pass.hpp>
#include <voxen/client/vulkan/surface.hpp>
#include <voxen/client/vulkan/swapchain.hpp>

#include <voxen/util/assert.hpp>
#include <voxen/util/log.hpp>

namespace voxen::client
{

VulkanFramebuffer::VulkanFramebuffer(const VkFramebufferCreateInfo &info) {
	auto &backend = VulkanBackend::backend();
	VkDevice device = *backend.device();
	VkResult result = backend.vkCreateFramebuffer(device, &info, VulkanHostAllocator::callbacks(), &m_framebuffer);
	if (result != VK_SUCCESS)
		throw VulkanException(result);
}

VulkanFramebuffer::~VulkanFramebuffer() noexcept {
	auto &backend = VulkanBackend::backend();
	VkDevice device = *backend.device();
	backend.vkDestroyFramebuffer(device, m_framebuffer, VulkanHostAllocator::callbacks());
}

VulkanFramebufferCollection::VulkanFramebufferCollection() :
	m_scene_framebuffer(createSceneFramebuffer())
{
	Log::debug("VulkanFramebufferCollection created successfully");
}

VulkanFramebuffer VulkanFramebufferCollection::createSceneFramebuffer() {
	Log::debug("Creating scene framebuffer");

	auto &backend = VulkanBackend::backend();
	auto *surface = backend.surface();
	auto *swapchain = backend.swapchain();
	vxAssert(surface != nullptr && swapchain != nullptr);
	vxAssert(backend.renderPassCollection() != nullptr);

	VkExtent2D frame_size = swapchain->imageExtent();
	VkFormat color_buffer_format = surface->format().format;

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
	// TODO: change back to 2
	attachments_info.attachmentImageInfoCount = 1;
	attachments_info.pAttachmentImageInfos = attachment_infos;

	VkFramebufferCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	info.pNext = &attachments_info;
	info.flags = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT;
	info.renderPass = backend.renderPassCollection()->mainRenderPass();
	// TODO: change back to 2
	info.attachmentCount = 1;
	info.width = frame_size.width;
	info.height = frame_size.height;
	info.layers = 1;
	return VulkanFramebuffer(info);
}

}
