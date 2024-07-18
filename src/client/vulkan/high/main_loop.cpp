#include <voxen/client/vulkan/high/main_loop.hpp>

#include <voxen/client/vulkan/algo/terrain_renderer.hpp>
#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/capabilities.hpp>
#include <voxen/client/vulkan/descriptor_set_layout.hpp>
#include <voxen/client/vulkan/framebuffer.hpp>
#include <voxen/gfx/vk/vk_device.hpp>
#include <voxen/gfx/vk/vk_physical_device.hpp>
#include <voxen/gfx/vk/vk_swapchain.hpp>
#include <voxen/util/log.hpp>

namespace voxen::client::vulkan
{

namespace
{

struct MainSceneUbo {
	glm::mat4 translated_world_to_clip;
	glm::vec3 world_position;
	float _pad0;
};

} // namespace

MainLoop::MainLoop() : m_fctx_ring(Backend::backend().device(), Config::NUM_CPU_PENDING_FRAMES)
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
	auto &swapchain = backend.swapchain();

	auto &fctx = m_fctx_ring.current();

	swapchain.acquireImage();

	VkCommandBuffer cmd_buf = fctx.commandBuffer();

	VkDescriptorSet main_scene_dset = createMainSceneDset(view);
	VkDescriptorSet frustum_cull_dset = fctx.allocateDescriptorSet(
		backend.descriptorSetLayoutCollection().terrainFrustumCullLayout());

	auto &terrain_renderer = backend.terrainRenderer();
	terrain_renderer.onNewWorldState(state);
	terrain_renderer.onFrameBegin(view, main_scene_dset, frustum_cull_dset);
	terrain_renderer.prepareResources(cmd_buf);
	terrain_renderer.launchFrustumCull(cmd_buf);

	// Prepare image layouts

	VkImageMemoryBarrier2 initialImageBarriers[2];

	initialImageBarriers[0] = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.pNext = nullptr,
		.srcStageMask = VK_PIPELINE_STAGE_2_NONE,
		.srcAccessMask = 0,
		.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = swapchain.currentImage(),
		.subresourceRange = VkImageSubresourceRange {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	initialImageBarriers[1] = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.pNext = nullptr,
		.srcStageMask = VK_PIPELINE_STAGE_2_NONE,
		.srcAccessMask = 0,
		.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
		.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = backend.framebufferCollection().sceneDepthStencilBuffer(),
		.subresourceRange = VkImageSubresourceRange {
			.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	const VkDependencyInfo initialDepInfo {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.pNext = nullptr,
		.dependencyFlags = 0,
		.memoryBarrierCount = 0,
		.pMemoryBarriers = nullptr,
		.bufferMemoryBarrierCount = 0,
		.pBufferMemoryBarriers = nullptr,
		.imageMemoryBarrierCount = std::size(initialImageBarriers),
		.pImageMemoryBarriers = initialImageBarriers,
	};

	backend.vkCmdPipelineBarrier2(cmd_buf, &initialDepInfo);

	// Main render pass

	const VkExtent2D frame_size = swapchain.imageExtent();

	VkRenderingAttachmentInfo color_rt_info = {};
	color_rt_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	color_rt_info.imageView = swapchain.currentImageRtv();
	color_rt_info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
	color_rt_info.resolveMode = VK_RESOLVE_MODE_NONE;
	color_rt_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_rt_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_rt_info.clearValue.color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

	VkRenderingAttachmentInfo depth_rt_info = {};
	depth_rt_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depth_rt_info.imageView = backend.framebufferCollection().sceneDepthStencilBufferView();
	depth_rt_info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
	depth_rt_info.resolveMode = VK_RESOLVE_MODE_NONE;
	depth_rt_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_rt_info.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_rt_info.clearValue.depthStencil = { 0.0f, 0 };

	VkRenderingAttachmentInfo stencil_rt_info = {};
	stencil_rt_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	stencil_rt_info.imageView = backend.framebufferCollection().sceneDepthStencilBufferView();
	stencil_rt_info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
	stencil_rt_info.resolveMode = VK_RESOLVE_MODE_NONE;
	stencil_rt_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	stencil_rt_info.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	stencil_rt_info.clearValue.depthStencil = { 0.0f, 0 };

	VkRenderingInfo render_info = {};
	render_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	render_info.renderArea.offset = { 0, 0 };
	render_info.renderArea.extent = frame_size;
	render_info.layerCount = 1;
	render_info.viewMask = 0;
	render_info.colorAttachmentCount = 1;
	render_info.pColorAttachments = &color_rt_info;
	render_info.pDepthAttachment = &depth_rt_info;
	if (VulkanUtils::hasStencilComponent(SCENE_DEPTH_STENCIL_BUFFER_FORMAT)) {
		render_info.pStencilAttachment = &stencil_rt_info;
	}

	backend.vkCmdBeginRendering(cmd_buf, &render_info);

	const VkViewport viewport {
		.x = 0.0f,
		.y = 0.0f,
		.width = float(frame_size.width),
		.height = float(frame_size.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	backend.vkCmdSetViewport(cmd_buf, 0, 1, &viewport);

	terrain_renderer.drawChunksInFrustum(cmd_buf);
	terrain_renderer.drawDebugChunkBorders(cmd_buf);

	backend.vkCmdEndRendering(cmd_buf);

	// Transfer from rendering to presentation layout

	const VkImageMemoryBarrier2 finalImageBarrier {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.pNext = nullptr,
		.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_NONE,
		.dstAccessMask = 0,
		.oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = swapchain.currentImage(),
		.subresourceRange = VkImageSubresourceRange {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	const VkDependencyInfo finalDepInfo {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.pNext = nullptr,
		.dependencyFlags = 0,
		.memoryBarrierCount = 0,
		.pMemoryBarriers = nullptr,
		.bufferMemoryBarrierCount = 0,
		.pBufferMemoryBarriers = nullptr,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &finalImageBarrier,
	};

	backend.vkCmdPipelineBarrier2(cmd_buf, &finalDepInfo);

	// Submit commands
	uint64_t timeline = m_fctx_ring.submitAndAdvance(swapchain.currentAcquireSemaphore(),
		swapchain.currentPresentSemaphore());

	swapchain.presentImage(timeline);
}

VkDescriptorSet MainLoop::createMainSceneDset(const GameView &view)
{
	auto &backend = Backend::backend();

	auto &fctx = m_fctx_ring.current();
	auto upload = fctx.allocateConstantUpload(sizeof(MainSceneUbo));

	auto *ubo_data = reinterpret_cast<MainSceneUbo *>(upload.host_mapped_span.data());

	ubo_data->translated_world_to_clip = view.translatedWorldToClip();
	ubo_data->world_position = view.cameraPosition();

	VkDescriptorSet dset = fctx.allocateDescriptorSet(backend.descriptorSetLayoutCollection().mainSceneLayout());

	const VkDescriptorBufferInfo buffer_info {
		.buffer = upload.buffer,
		.offset = upload.offset,
		.range = sizeof(MainSceneUbo),
	};

	const VkWriteDescriptorSet write {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.pNext = nullptr,
		.dstSet = dset,
		.dstBinding = 0,
		.dstArrayElement = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.pImageInfo = nullptr,
		.pBufferInfo = &buffer_info,
		.pTexelBufferView = nullptr,
	};

	backend.vkUpdateDescriptorSets(backend.device().handle(), 1, &write, 0, nullptr);
	return dset;
}

} // namespace voxen::client::vulkan
