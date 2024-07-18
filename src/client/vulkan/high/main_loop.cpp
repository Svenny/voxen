#include <voxen/client/vulkan/high/main_loop.hpp>

#include <voxen/client/vulkan/algo/terrain_renderer.hpp>
#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/capabilities.hpp>
#include <voxen/client/vulkan/descriptor_manager.hpp>
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

MainLoop::MainLoop()
	: m_graphics_command_pool(Backend::backend().device().info().main_queue_family)
	, m_graphics_command_buffers(m_graphics_command_pool.allocateCommandBuffers(Config::NUM_CPU_PENDING_FRAMES))
{
	Backend &backend = Backend::backend();
	const VkDeviceSize align = backend.device().physInfo().props.properties.limits.minUniformBufferOffsetAlignment;
	const VkDeviceSize ubo_size = VulkanUtils::alignUp(sizeof(MainSceneUbo), align);

	m_main_scene_ubo = FatVkBuffer(
		VkBufferCreateInfo {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.size = ubo_size * Config::NUM_CPU_PENDING_FRAMES,
			.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices = nullptr,
		},
		DeviceMemoryUseCase::FastUpload);

	m_main_scene_ubo.allocation().tryHostMap();
	assert(m_main_scene_ubo.allocation().hostPointer());

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

	size_t pending_frame_id = m_frame_id % Config::NUM_CPU_PENDING_FRAMES;
	backend.device().waitForTimeline(gfx::vk::Device::QueueMain, m_submit_timelines[pending_frame_id]);

	// Advance current set ID when CPU resources for current frame
	// are not used anymore but before any CPU work has started
	backend.descriptorManager().startNewFrame();

	swapchain.acquireImage();

	VkCommandBuffer cmd_buf = m_graphics_command_buffers[pending_frame_id];
	VkCommandBufferBeginInfo cmd_begin_info = {};
	cmd_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmd_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VkResult result = backend.vkBeginCommandBuffer(cmd_buf, &cmd_begin_info);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkBeginCommandBuffer");
	}

	updateMainSceneUbo(view);

	auto &terrain_renderer = backend.terrainRenderer();
	terrain_renderer.onNewWorldState(state);
	terrain_renderer.onFrameBegin(view);
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

	result = backend.vkEndCommandBuffer(cmd_buf);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkEndCommandBuffer");
	}

	uint64_t timeline = backend.device().submitCommands({
		.wait_binary_semaphore = swapchain.currentAcquireSemaphore(),
		.cmds = std::span(&cmd_buf, 1),
		.signal_binary_semaphore = swapchain.currentPresentSemaphore(),
	});

	m_submit_timelines[pending_frame_id] = timeline;
	swapchain.presentImage(timeline);

	m_frame_id++;
}

void MainLoop::updateMainSceneUbo(const GameView &view)
{
	auto &backend = Backend::backend();

	const VkDeviceSize align = backend.device().physInfo().props.properties.limits.minUniformBufferOffsetAlignment;
	const VkDeviceSize ubo_size = VulkanUtils::alignUp(sizeof(MainSceneUbo), align);

	const uint32_t set_id = backend.descriptorManager().setId();
	const uintptr_t ubo_data_offset = set_id * ubo_size;

	MainSceneUbo *ubo_data = reinterpret_cast<MainSceneUbo *>(
		uintptr_t(m_main_scene_ubo.allocation().hostPointer()) + ubo_data_offset);

	ubo_data->translated_world_to_clip = view.translatedWorldToClip();
	ubo_data->world_position = view.cameraPosition();

	const VkDescriptorBufferInfo buffer_info {
		.buffer = m_main_scene_ubo,
		.offset = ubo_data_offset,
		.range = sizeof(MainSceneUbo),
	};

	const VkWriteDescriptorSet write {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.pNext = nullptr,
		.dstSet = backend.descriptorManager().mainSceneSet(),
		.dstBinding = 0,
		.dstArrayElement = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.pImageInfo = nullptr,
		.pBufferInfo = &buffer_info,
		.pTexelBufferView = nullptr,
	};

	backend.vkUpdateDescriptorSets(backend.device().handle(), 1, &write, 0, nullptr);
}

} // namespace voxen::client::vulkan
