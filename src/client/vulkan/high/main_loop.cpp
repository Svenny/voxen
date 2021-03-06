#include <voxen/client/vulkan/high/main_loop.hpp>

#include <voxen/client/vulkan/algo/debug_octree.hpp>
#include <voxen/client/vulkan/algo/terrain_simple.hpp>
#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>
#include <voxen/client/vulkan/framebuffer.hpp>
#include <voxen/client/vulkan/physical_device.hpp>
#include <voxen/client/vulkan/render_pass.hpp>
#include <voxen/client/vulkan/swapchain.hpp>

#include <voxen/util/log.hpp>

namespace voxen::client::vulkan
{

MainLoop::PendingFrameSyncs::PendingFrameSyncs() : render_done_fence(true) {}

MainLoop::MainLoop()
	: m_image_guard_fences(Backend::backend().swapchain().numImages(), VK_NULL_HANDLE),
	m_graphics_command_pool(Backend::backend().physicalDevice().graphicsQueueFamily()),
	m_graphics_command_buffers(m_graphics_command_pool.allocateCommandBuffers(MAX_PENDING_FRAMES))
{
	Log::debug("MainLoop created successfully");
}

MainLoop::~MainLoop() noexcept
{
	Log::debug("Destroying MainLoop");
}

void MainLoop::drawFrame(const WorldState &state, const GameView &view)
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device();
	auto &swapchain = backend.swapchain();

	size_t pending_frame_id = m_frame_id % MAX_PENDING_FRAMES;
	VkSemaphore frame_acquired_semaphore = m_pending_frame_syncs[pending_frame_id].frame_acquired_semaphore;
	VkSemaphore render_done_semaphore = m_pending_frame_syncs[pending_frame_id].render_done_semaphore;
	VkFence render_done_fence = m_pending_frame_syncs[pending_frame_id].render_done_fence;

	VkResult result = backend.vkWaitForFences(device, 1, &render_done_fence, VK_TRUE, UINT64_MAX);
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkWaitForFences");

	uint32_t swapchain_image_id = swapchain.acquireImage(frame_acquired_semaphore);

	{
		VkFence image_guard_fence = m_image_guard_fences[swapchain_image_id];
		if (image_guard_fence != VK_NULL_HANDLE) {
			result = backend.vkWaitForFences(device, 1, &image_guard_fence, VK_TRUE, UINT64_MAX);
			if (result != VK_SUCCESS)
				throw VulkanException(result, "vkWaitForFences");
		}
		m_image_guard_fences[swapchain_image_id] = render_done_fence;
	}

	result = backend.vkResetFences(device, 1, &render_done_fence);
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkResetFences");

	VkCommandBuffer cmd_buf = m_graphics_command_buffers[pending_frame_id];
	VkCommandBufferBeginInfo cmd_begin_info = {};
	cmd_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmd_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	result = backend.vkBeginCommandBuffer(cmd_buf, &cmd_begin_info);
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkBeginCommandBuffer");

	VkRenderPassAttachmentBeginInfo attachment_info = {};
	attachment_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO;
	VkImageView attachments[2] = {
		swapchain.imageView(swapchain_image_id),
		backend.framebufferCollection().sceneDepthStencilBufferView()
	};
	attachment_info.attachmentCount = std::size(attachments);
	attachment_info.pAttachments = attachments;

	const VkExtent2D frame_size = swapchain.imageExtent();

	VkRenderPassBeginInfo render_begin_info = {};
	render_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_begin_info.pNext = &attachment_info;
	render_begin_info.renderPass = backend.renderPassCollection().mainRenderPass();
	render_begin_info.framebuffer = backend.framebufferCollection().sceneFramebuffer();
	render_begin_info.renderArea.offset = { 0, 0 };
	render_begin_info.renderArea.extent = frame_size;
	render_begin_info.clearValueCount = 2;
	VkClearValue clear_values[2];
	clear_values[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
	clear_values[1].depthStencil = { 0.0f, 0 };
	render_begin_info.pClearValues = clear_values;
	backend.vkCmdBeginRenderPass(cmd_buf, &render_begin_info, VK_SUBPASS_CONTENTS_INLINE);

	const VkViewport viewport {
		.x = 0.0f,
		.y = 0.0f,
		.width = float(frame_size.width),
		.height = float(frame_size.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};
	backend.vkCmdSetViewport(cmd_buf, 0, 1, &viewport);

	backend.algoTerrainSimple().executePass(cmd_buf, state, view);
	backend.algoDebugOctree().executePass(cmd_buf, view);

	backend.vkCmdEndRenderPass(cmd_buf);

	result = backend.vkEndCommandBuffer(cmd_buf);
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkEndCommandBuffer");

	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = &frame_acquired_semaphore;
	VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	submit_info.pWaitDstStageMask = &wait_stage;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cmd_buf;
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = &render_done_semaphore;

	VkQueue queue = backend.device().graphicsQueue();
	result = backend.vkQueueSubmit(queue, 1, &submit_info, render_done_fence);
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkQueueSubmit");

	swapchain.presentImage(swapchain_image_id, render_done_semaphore);

	m_frame_id++;
}

}
