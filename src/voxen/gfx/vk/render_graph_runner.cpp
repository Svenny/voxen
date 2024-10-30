#include <voxen/gfx/vk/render_graph_runner.hpp>

#include <voxen/client/vulkan/common.hpp>
#include <voxen/gfx/vk/render_graph_execution.hpp>
#include <voxen/util/error_condition.hpp>
#include <voxen/util/log.hpp>

#include "render_graph_private.hpp"

#include <vma/vk_mem_alloc.h>

#include <cassert>

namespace voxen::gfx::vk
{

// TODO: there parts are not yet moved to voxen/gfx/vk
using client::vulkan::VulkanException;

RenderGraphRunner::~RenderGraphRunner() noexcept = default;

template<>
void RenderGraphRunner::visitCommand(RenderGraphExecution &exec, RenderGraphPrivate::BarrierCommand &cmd)
{
	auto bufs = std::make_unique<VkBufferMemoryBarrier2[]>(cmd.buffer.size());
	auto imgs = std::make_unique<VkImageMemoryBarrier2[]>(cmd.image.size());

	for (size_t i = 0; i < cmd.buffer.size(); i++) {
		bufs[i] = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
			.pNext = nullptr,
			.srcStageMask = cmd.buffer[i].src_stages,
			.srcAccessMask = cmd.buffer[i].src_access,
			.dstStageMask = cmd.buffer[i].dst_stages,
			.dstAccessMask = cmd.buffer[i].dst_access,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.buffer = cmd.buffer[i].buffer->handle,
			.offset = 0,
			.size = VK_WHOLE_SIZE,
		};
	}

	for (size_t i = 0; i < cmd.image.size(); i++) {
		imgs[i] = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.pNext = nullptr,
			.srcStageMask = cmd.image[i].src_stages,
			.srcAccessMask = cmd.image[i].src_access,
			.dstStageMask = cmd.image[i].dst_stages,
			.dstAccessMask = cmd.image[i].dst_access,
			.oldLayout = cmd.image[i].old_layout,
			.newLayout = cmd.image[i].new_layout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = cmd.image[i].image->handle,
			.subresourceRange = cmd.image[i].subresource,
		};
	}

	VkDependencyInfo dep_info {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.pNext = nullptr,
		.dependencyFlags = 0,
		.memoryBarrierCount = 0,
		.pMemoryBarriers = nullptr,
		.bufferMemoryBarrierCount = uint32_t(cmd.buffer.size()),
		.pBufferMemoryBarriers = bufs.get(),
		.imageMemoryBarrierCount = uint32_t(cmd.image.size()),
		.pImageMemoryBarriers = imgs.get(),
	};

	m_device.dt().vkCmdPipelineBarrier2(exec.frameContext().commandBuffer(), &dep_info);
}

template<>
void RenderGraphRunner::visitCommand(RenderGraphExecution &exec, RenderGraphPrivate::RenderPassCommand &cmd)
{
	VkRenderingAttachmentInfo color_targets[Consts::GRAPH_MAX_RENDER_TARGETS];

	uint32_t color_target_count = 0;

	for (size_t i = 0; i < Consts::GRAPH_MAX_RENDER_TARGETS; i++) {
		if (!cmd.targets[i].resource) {
			break;
		}

		++color_target_count;

		color_targets[i] = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.pNext = nullptr,
			.imageView = cmd.targets[i].resource->handle,
			.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
			.resolveMode = VK_RESOLVE_MODE_NONE,
			.resolveImageView = VK_NULL_HANDLE,
			.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.loadOp = cmd.targets[i].load_op,
			.storeOp = cmd.targets[i].store_op,
			.clearValue = { .color = cmd.targets[i].clear_value },
		};
	}

	VkRenderingAttachmentInfo depth_target;
	VkRenderingAttachmentInfo stencil_target;

	bool ztarget = cmd.ds_target.resource
		&& !!(cmd.ds_target.resource->create_info.subresourceRange.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT);
	bool starget = cmd.ds_target.resource
		&& !!(cmd.ds_target.resource->create_info.subresourceRange.aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT);

	if (ztarget) {
		depth_target = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.pNext = nullptr,
			.imageView = cmd.ds_target.resource->handle,
			.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
			.resolveMode = VK_RESOLVE_MODE_NONE,
			.resolveImageView = VK_NULL_HANDLE,
			.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.loadOp = cmd.ds_target.load_op,
			.storeOp = cmd.ds_target.store_op,
			.clearValue = { .depthStencil = cmd.ds_target.clear_value },
		};
	}

	if (starget) {
		stencil_target = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.pNext = nullptr,
			.imageView = cmd.ds_target.resource->handle,
			.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
			.resolveMode = VK_RESOLVE_MODE_NONE,
			.resolveImageView = VK_NULL_HANDLE,
			.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.loadOp = cmd.ds_target.load_op,
			.storeOp = cmd.ds_target.store_op,
			.clearValue = { .depthStencil = cmd.ds_target.clear_value },
		};
	}

	// Determine render area from any render target (they must all match)
	VkExtent3D extent;
	uint32_t view_mip;
	uint32_t layer_count;
	if (cmd.targets[0].resource) {
		assert(cmd.targets[0].resource->image);
		extent = cmd.targets[0].resource->image->create_info.extent;
		view_mip = cmd.targets[0].resource->create_info.subresourceRange.baseMipLevel;
		layer_count = cmd.targets[0].resource->create_info.subresourceRange.layerCount;
	} else {
		assert(cmd.ds_target.resource && cmd.ds_target.resource->image);
		extent = cmd.ds_target.resource->image->create_info.extent;
		view_mip = cmd.ds_target.resource->create_info.subresourceRange.baseMipLevel;
		layer_count = cmd.ds_target.resource->create_info.subresourceRange.layerCount;
	}

	extent.width = std::max(1u, extent.width >> view_mip);
	extent.height = std::max(1u, extent.height >> view_mip);

	VkRenderingInfo rendering_info {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.pNext = nullptr,
		.flags = 0,
		.renderArea = { .offset = {0, 0}, .extent = {extent.width, extent.height}, },
		.layerCount = layer_count,
		.viewMask = 0,
		.colorAttachmentCount = color_target_count,
		.pColorAttachments = color_targets,
		.pDepthAttachment = ztarget ? &depth_target : nullptr,
		.pStencilAttachment = starget ? &stencil_target : nullptr,
	};

	VkCommandBuffer cmd_buf = exec.frameContext().commandBuffer();
	auto scope = m_device.debug().cmdPushLabel(cmd_buf, cmd.name.c_str(), Consts::RENDER_PASS_LABEL_COLOR);

	m_device.dt().vkCmdBeginRendering(cmd_buf, &rendering_info);
	cmd.callback(*m_graph, exec);
	m_device.dt().vkCmdEndRendering(cmd_buf);
}

template<>
void RenderGraphRunner::visitCommand(RenderGraphExecution &exec, RenderGraphPrivate::ComputePassCommand &cmd)
{
	VkCommandBuffer cmd_buf = exec.frameContext().commandBuffer();
	auto scope = m_device.debug().cmdPushLabel(cmd_buf, cmd.name.c_str(), Consts::COMPUTE_PASS_LABEL_COLOR);

	cmd.callback(*m_graph, exec);
}

RenderGraphRunner::RenderGraphRunner(Device &device, os::GlfwWindow &window) : m_device(device), m_window(window)
{
	m_private = std::make_shared<RenderGraphPrivate>(m_device, m_window);
}

void RenderGraphRunner::attachGraph(std::shared_ptr<IRenderGraph> graph)
{
	// Drop all resources/commands for previous graph
	m_private->clear();

	m_graph = std::move(graph);

	rebuildGraph();
}

void RenderGraphRunner::rebuildGraph()
{
	if (!m_graph) {
		return;
	}

	// Drop all previously created resources/commands
	m_private->clear();

	RenderGraphBuilder bld(*m_private);
	m_graph->rebuild(bld);

	// Transition swapchain image to presentable layout
	bld.resolveImageHazards(m_private->output_rtv, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_ACCESS_2_NONE,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, false);

	finalizeRebuild();
}

void RenderGraphRunner::executeGraph()
{
	assert(m_graph);

	if (m_private->fctx_ring.badState()) [[unlikely]] {
		Log::error("Frame context ring is in bad state, can't execture render graphs anymore!");
		throw Exception::fromError(VoxenErrc::GfxFailure, "render graph frame context ring broken");
	}

	auto &swapchain = m_private->swapchain;
	if (swapchain.badState()) [[unlikely]] {
		Log::error("Swapchain is in bad state, can't execute render graphs anymore!");
		throw Exception::fromError(VoxenErrc::GfxFailure, "render graph swapchain broken");
	}

	swapchain.acquireImage();

	// Check for swapchain image changes (strictly after `acquireImage()`)
	VkFormat cur_format = swapchain.imageFormat();
	VkFormat last_format = m_private->last_known_swapchain_format;
	VkExtent2D cur_res = swapchain.imageExtent();
	VkExtent2D last_res = m_private->last_known_swapchain_resolution;

	if (cur_format != last_format || cur_res.width != last_res.width || cur_res.height != last_res.height) {
		Log::info("Swapchain image format/resolution changed, rebuilding the render graph");
		m_private->last_known_swapchain_format = cur_format;
		m_private->last_known_swapchain_resolution = cur_res;
		rebuildGraph();
	}

	publishResourceHandles();

	RenderGraphExecution exec(*m_private);
	m_graph->beginExecution(exec);

	for (auto &cmd : m_private->commands) {
		// Dispatch to type-specific command handlers
		std::visit([&](auto &&arg) { visitCommand(exec, arg); }, cmd);
	}

	m_graph->endExecution(exec);

	uint64_t timeline = m_private->fctx_ring.submitAndAdvance(swapchain.currentAcquireSemaphore(),
		swapchain.currentPresentSemaphore());
	swapchain.presentImage(timeline);
}

void RenderGraphRunner::finalizeRebuild()
{
	// Normalize create infos for double-buffered resources
	for (auto &image : m_private->images) {
		if (!image.temporal_sibling) {
			continue;
		}

		image.create_info.usage |= image.temporal_sibling->create_info.usage;

		for (auto &view : image.views) {
			if (!view.temporal_sibling) {
				continue;
			}

			view.usage_create_info.usage |= view.temporal_sibling->usage_create_info.usage;
		}
	}

	// Allocate resources (except dynamic-sized buffers)
	VmaAllocator vma = m_device.vma();

	for (auto &buffer : m_private->buffers) {
		if (buffer.dynamic_sized) {
			continue;
		}

		VmaAllocationCreateInfo vma_info {};
		vma_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

		VkResult res = vmaCreateBuffer(vma, &buffer.create_info, &vma_info, &buffer.handle, &buffer.alloc, nullptr);
		if (res != VK_SUCCESS) {
			throw VulkanException(res, "vmaCreateBuffer");
		}

		m_device.setObjectName(buffer.handle, buffer.name.c_str());
	}

	for (auto &image : m_private->images) {
		VkImageFormatListCreateInfo format_list {
			.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO,
			.pNext = nullptr,
			.viewFormatCount = 0,
			.pViewFormats = nullptr,
		};

		std::vector<VkFormat> formats;
		for (auto &view : image.views) {
			VkFormat fmt = view.create_info.format;

			if (fmt == image.create_info.format) {
				// No format reinterpret, skip it
				continue;
			}

			auto iter = std::find(formats.begin(), formats.end(), fmt);
			if (iter == formats.end()) {
				formats.emplace_back(fmt);
			}
		}

		if (!formats.empty()) {
			// We didn't add base image format, do it now
			formats.emplace_back(image.create_info.format);

			format_list.viewFormatCount = static_cast<uint32_t>(formats.size());
			format_list.pViewFormats = formats.data();

			image.create_info.pNext = &format_list;
			image.create_info.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
		}

		VmaAllocationCreateInfo vma_info {};
		vma_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

		VkResult res = vmaCreateImage(vma, &image.create_info, &vma_info, &image.handle, &image.alloc, nullptr);
		if (res != VK_SUCCESS) {
			throw VulkanException(res, "vmaCreateImage");
		}

		m_device.setObjectName(image.handle, image.name.c_str());

		// Remove possible `format_list` pointer to protect from accidental dangling pointers
		image.create_info.pNext = nullptr;

		for (auto &view : image.views) {
			view.create_info.image = image.handle;

			res = m_device.dt().vkCreateImageView(m_device.handle(), &view.create_info, nullptr, &view.handle);
			if (res != VK_SUCCESS) {
				throw VulkanException(res, "vkCreateImageView");
			}

			m_device.setObjectName(view.handle, view.name.c_str());
		}
	}
}

void RenderGraphRunner::publishResourceHandles()
{
	m_private->output_image.handle = m_private->swapchain.currentImage();
	assert(m_private->output_image.handle != VK_NULL_HANDLE);

	m_private->output_rtv.handle = m_private->swapchain.currentImageRtv();
	assert(m_private->output_rtv.handle != VK_NULL_HANDLE);

	for (auto &buffer : m_private->buffers) {
		VkBuffer handle = buffer.handle;

		if (buffer.dynamic_sized) {
			// Publish NULL handle to prevent using this buffer without providing the size.
			// Actual handle will be published by `setDynamicBufferSize()`.
			buffer.used_size = 0;
			handle = VK_NULL_HANDLE;
		}

		if (buffer.resource) {
			buffer.resource->setHandle(handle);
		}
	}

	for (auto &image : m_private->images) {
		// Swap handles of double-buffered images. Compare pointers to swap only once.
		if (image.temporal_sibling && (&image < image.temporal_sibling)) {
			std::swap(image.handle, image.temporal_sibling->handle);
		}

		if (image.resource) {
			image.resource->setHandle(image.handle);
		}

		for (auto &view : image.views) {
			// Swap handles of double-buffered views. Compare pointers to swap only once.
			if (view.temporal_sibling && (&view < view.temporal_sibling)) {
				std::swap(view.handle, view.temporal_sibling->handle);
			}

			if (view.resource) {
				view.resource->setHandle(view.handle);
			}
		}
	}
}

} // namespace voxen::gfx::vk
