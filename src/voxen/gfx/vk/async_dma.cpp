#include <voxen/gfx/vk/async_dma.hpp>

#include <voxen/client/vulkan/common.hpp>
#include <voxen/gfx/vk/vk_device.hpp>

#include <extras/defer.hpp>

#include <vma/vk_mem_alloc.h>

#include <cstring>

namespace voxen::gfx::vk
{

// TODO: there parts are not yet moved to voxen/gfx/vk
using client::vulkan::VulkanException;

struct AsyncDma::TransferCommand {
	VkBuffer src_buffer;
	VkImage dst_image;

	VkBufferImageCopy buffer_image_region;
};

struct AsyncDma::TransferCommandB {
	VkBuffer src_buffer;
	VkBuffer dst_buffer;
	VkDeviceSize dst_offset;
	VkDeviceSize size;
};

AsyncDma::AsyncDma(Device &dev) : m_dev(dev), m_dma_queue_family(dev.info().dma_queue_family)
{
	VkCommandPoolCreateInfo create_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
		.queueFamilyIndex = m_dma_queue_family,
	};

	VkResult res = dev.dt().vkCreateCommandPool(dev.handle(), &create_info, nullptr, &m_cmd_pool);
	if (res != VK_SUCCESS) [[likely]] {
		throw VulkanException(res, "vkCreateCommandPool");
	}
	defer_fail { dev.dt().vkDestroyCommandPool(dev.handle(), m_cmd_pool, nullptr); };

	dev.setObjectName(m_cmd_pool, "dma/cmd_pool");

	VkCommandBufferAllocateInfo alloc_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext = nullptr,
		.commandPool = m_cmd_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = MAX_CMD_BUFFERS,
	};

	res = dev.dt().vkAllocateCommandBuffers(dev.handle(), &alloc_info, m_cmd_buffers);
	if (res != VK_SUCCESS) [[likely]] {
		throw VulkanException(res, "vkAllocateCommandBuffers");
	}

	for (uint32_t i = 0; i < MAX_CMD_BUFFERS; i++) {
		char buf[32];
		snprintf(buf, std::size(buf), "dma/cmd_buffer_%u", i + 1);
		dev.setObjectName(m_cmd_buffers[i], buf);
	}
}

AsyncDma::~AsyncDma() noexcept
{
	m_dev.enqueueDestroy(m_cmd_pool);

	for (auto &[buffer, alloc] : m_scratch_buffers) {
		m_dev.enqueueDestroy(buffer, alloc);
	}
}

void AsyncDma::transfer(BufferToImageUpload upload)
{
	if (upload.buffer.empty()) [[unlikely]] {
		return;
	}

	// TODO: quite hacky, limit cmdbuf size to something reasonable
	if (m_tx_cmds.size() > 256) {
		flush();
	}

	auto [scratch_handle, scratch_ptr] = allocateScratchBuffer(upload.buffer.size_bytes());
	memcpy(scratch_ptr, upload.buffer.data(), upload.buffer.size_bytes());

	bool ownership_transfer = upload.original_queue_family != VK_QUEUE_FAMILY_IGNORED
		&& upload.original_queue_family != m_dma_queue_family;

	uint32_t outer_queue_family = ownership_transfer ? upload.original_queue_family : VK_QUEUE_FAMILY_IGNORED;
	uint32_t inner_queue_family = ownership_transfer ? m_dma_queue_family : VK_QUEUE_FAMILY_IGNORED;

	if (ownership_transfer || upload.initial_layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) [[likely]] {
		m_pre_barriers.emplace_back(VkImageMemoryBarrier2 {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.pNext = nullptr,
			.srcStageMask = 0,
			.srcAccessMask = 0,
			.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
			.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
			.oldLayout = upload.initial_layout,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.srcQueueFamilyIndex = outer_queue_family,
			.dstQueueFamilyIndex = inner_queue_family,
			.image = upload.image,
			.subresourceRange = {
				.aspectMask = upload.image_subresource.aspectMask,
				.baseMipLevel = upload.image_subresource.mipLevel,
				.levelCount = 1,
				.baseArrayLayer = upload.image_subresource.baseArrayLayer,
				.layerCount = upload.image_subresource.layerCount,
			},
		});
	}

	m_tx_cmds.emplace_back(TransferCommand {
		.src_buffer = scratch_handle,
		.dst_image = upload.image,
		.buffer_image_region = {
			.bufferOffset = 0,
			.bufferRowLength = upload.buffer_row_length,
			.bufferImageHeight = upload.buffer_image_height,
			.imageSubresource = upload.image_subresource,
			.imageOffset = upload.image_offset,
			.imageExtent = upload.image_extent,
		},
	});

	if (ownership_transfer || upload.final_layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) [[likely]] {
		m_post_barriers.emplace_back(VkImageMemoryBarrier2 {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.pNext = nullptr,
			.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
			.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
			.dstStageMask = 0,
			.dstAccessMask = 0,
			.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.newLayout = upload.final_layout,
			.srcQueueFamilyIndex = inner_queue_family,
			.dstQueueFamilyIndex = outer_queue_family,
			.image = upload.image,
			.subresourceRange = {
				.aspectMask = upload.image_subresource.aspectMask,
				.baseMipLevel = upload.image_subresource.mipLevel,
				.levelCount = 1,
				.baseArrayLayer = upload.image_subresource.baseArrayLayer,
				.layerCount = upload.image_subresource.layerCount,
			},
		});
	}
}

void AsyncDma::transfer(BufferToBufferUpload upload)
{
	if (upload.src_buffer.empty()) [[unlikely]] {
		return;
	}

	// TODO: quite hacky, limit cmdbuf size to something reasonable
	if (m_txb_cmds.size() > 256) {
		flush();
	}

	auto [scratch_handle, scratch_ptr] = allocateScratchBuffer(upload.src_buffer.size_bytes());
	memcpy(scratch_ptr, upload.src_buffer.data(), upload.src_buffer.size_bytes());

	m_txb_cmds.emplace_back(TransferCommandB {
		.src_buffer = scratch_handle,
		.dst_buffer = upload.dst_buffer,
		.dst_offset = upload.dst_offset,
		.size = upload.src_buffer.size_bytes(),
	});
}

uint64_t AsyncDma::flush()
{
	if (m_pre_barriers.empty() && m_tx_cmds.empty() && m_txb_cmds.empty() && m_post_barriers.empty()) [[unlikely]] {
		// No need to begin/end a cmdbuf for nothing, return the last submitted timeline
		uint32_t last_submitted = (m_current_cmd_buffer + MAX_CMD_BUFFERS - 1) % MAX_CMD_BUFFERS;
		return m_cmd_submit_timelines[last_submitted];
	}

	m_dev.waitForTimeline(Device::QueueDma, m_cmd_submit_timelines[m_current_cmd_buffer]);

	VkCommandBuffer cmd_buf = m_cmd_buffers[m_current_cmd_buffer];

	{
		auto &ddt = m_dev.dt();

		VkCommandBufferBeginInfo begin_info {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = nullptr,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			.pInheritanceInfo = nullptr,
		};

		VkResult res = ddt.vkBeginCommandBuffer(cmd_buf, &begin_info);
		if (res != VK_SUCCESS) [[unlikely]] {
			throw VulkanException(res, "vkBeginCommandBuffer");
		}

		VkDependencyInfo dependency {
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.pNext = nullptr,
			.dependencyFlags = 0,
			.memoryBarrierCount = 0,
			.pMemoryBarriers = nullptr,
			.bufferMemoryBarrierCount = 0,
			.pBufferMemoryBarriers = nullptr,
			.imageMemoryBarrierCount = uint32_t(m_pre_barriers.size()),
			.pImageMemoryBarriers = m_pre_barriers.data(),
		};

		if (!m_pre_barriers.empty()) [[likely]] {
			ddt.vkCmdPipelineBarrier2(cmd_buf, &dependency);
		}

		for (const auto &cmd : m_tx_cmds) {
			ddt.vkCmdCopyBufferToImage(cmd_buf, cmd.src_buffer, cmd.dst_image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cmd.buffer_image_region);
		}

		for (const auto &cmd : m_txb_cmds) {
			VkBufferCopy region {
				.srcOffset = 0,
				.dstOffset = cmd.dst_offset,
				.size = cmd.size,
			};
			ddt.vkCmdCopyBuffer(cmd_buf, cmd.src_buffer, cmd.dst_buffer, 1, &region);
		}

		if (!m_post_barriers.empty()) [[likely]] {
			dependency.imageMemoryBarrierCount = uint32_t(m_post_barriers.size());
			dependency.pImageMemoryBarriers = m_post_barriers.data();
			ddt.vkCmdPipelineBarrier2(cmd_buf, &dependency);
		}

		res = ddt.vkEndCommandBuffer(cmd_buf);
		if (res != VK_SUCCESS) [[unlikely]] {
			throw VulkanException(res, "vkEndCommandBuffer");
		}
	}

	uint64_t timeline = m_dev.submitCommands({
		.queue = Device::QueueDma,
		.cmds = std::span(&cmd_buf, 1),
	});

	m_cmd_submit_timelines[m_current_cmd_buffer] = timeline;
	m_current_cmd_buffer = (m_current_cmd_buffer + 1) % MAX_CMD_BUFFERS;

	// Yeah, forget about them immediately!
	for (auto &[buffer, alloc] : m_scratch_buffers) {
		m_dev.enqueueDestroy(buffer, alloc);
	}

	m_scratch_buffers.clear();
	m_pre_barriers.clear();
	m_tx_cmds.clear();
	m_txb_cmds.clear();
	m_post_barriers.clear();

	return timeline;
}

std::pair<VkBuffer, void *> AsyncDma::allocateScratchBuffer(VkDeviceSize size)
{
	VkBufferCreateInfo buffer_create_info {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.size = size,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
	};

	VmaAllocationCreateInfo alloc_create_info {};
	alloc_create_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation alloc = VK_NULL_HANDLE;

	VmaAllocationInfo alloc_info {};

	VkResult res = vmaCreateBuffer(m_dev.vma(), &buffer_create_info, &alloc_create_info, &buffer, &alloc, &alloc_info);
	if (res != VK_SUCCESS) [[unlikely]] {
		throw VulkanException(res, "vmaCreateBuffer");
	}

	m_scratch_buffers.emplace_back(buffer, alloc);
	return { buffer, alloc_info.pMappedData };
}

} // namespace voxen::gfx::vk
