#include <voxen/client/vulkan/render_pass.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>
#include <voxen/client/vulkan/surface.hpp>

#include <voxen/util/assert.hpp>
#include <voxen/util/log.hpp>

namespace voxen::client::vulkan
{

RenderPass::RenderPass(const VkRenderPassCreateInfo &info) {
	auto &backend = VulkanBackend::backend();
	VkDevice device = *backend.device();
	VkResult result = backend.vkCreateRenderPass(device, &info, VulkanHostAllocator::callbacks(), &m_render_pass);
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkCreateRenderPass");
}

RenderPass::~RenderPass() noexcept {
	auto &backend = VulkanBackend::backend();
	VkDevice device = *backend.device();
	backend.vkDestroyRenderPass(device, m_render_pass, VulkanHostAllocator::callbacks());
}

RenderPassCollection::RenderPassCollection() :
	m_swapchain_color_buffer(describeSwapchainColorBuffer()),
	m_scene_depth_stencil_buffer(describeSceneDepthStencilBuffer()),
	m_main_render_pass(createMainRenderPass())
{
	Log::debug("RenderPassCollection created successfully");
}

VkAttachmentDescription RenderPassCollection::describeSwapchainColorBuffer() {
	auto &backend = VulkanBackend::backend();
	vxAssert(backend.surface() != nullptr);

	VkAttachmentDescription desc = {};
	desc.format = VulkanBackend::backend().surface()->format().format;
	desc.samples = VK_SAMPLE_COUNT_1_BIT;
	desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	desc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	return desc;
}

VkAttachmentDescription RenderPassCollection::describeSceneDepthStencilBuffer() {
	VkAttachmentDescription desc = {};
	desc.format = SCENE_DEPTH_STENCIL_BUFFER_FORMAT;
	desc.samples = VK_SAMPLE_COUNT_1_BIT;
	desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	desc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	// Don't forget to change to `CLEAR` if stencil becomes used
	desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	desc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	return desc;
}

RenderPass RenderPassCollection::createMainRenderPass() {
	Log::debug("Creating main render pass");

	VkAttachmentDescription attachments[2] = { m_swapchain_color_buffer, m_scene_depth_stencil_buffer };

	VkAttachmentReference color_buffer_ref = {};
	color_buffer_ref.attachment = 0;
	color_buffer_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkAttachmentReference depth_stencil_buffer_ref = {};
	depth_stencil_buffer_ref.attachment = 1;
	depth_stencil_buffer_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass_desc = {};
	subpass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass_desc.colorAttachmentCount = 1;
	subpass_desc.pColorAttachments = &color_buffer_ref;
	// TODO: uncomment me
	(void)depth_stencil_buffer_ref;
	//subpass_desc.pDepthStencilAttachment = &depth_stencil_buffer_ref;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	// TODO: change back to 2
	info.attachmentCount = 1;
	info.pAttachments = attachments;
	info.subpassCount = 1;
	info.pSubpasses = &subpass_desc;
	info.dependencyCount = 1;
	info.pDependencies = &dependency;
	return RenderPass(info);
}

}
