#pragma once

#include <voxen/gfx/vk/vma_fwd.hpp>

#include <vulkan/vulkan.h>

#include <span>
#include <vector>

namespace voxen::gfx::vk
{

class Device;

class AsyncDma {
public:
	constexpr static uint32_t MAX_CMD_BUFFERS = 8;

	struct BufferToImageUpload {
		std::span<const std::byte> buffer;
		uint32_t buffer_row_length;
		uint32_t buffer_image_height;
		VkImageSubresourceLayers image_subresource;
		VkOffset3D image_offset;
		VkExtent3D image_extent;

		VkImage image;
		VkImageLayout initial_layout;
		VkImageLayout final_layout;
		uint32_t original_queue_family;
	};

	struct BufferToBufferUpload {
		std::span<const std::byte> src_buffer;

		VkBuffer dst_buffer;
		VkDeviceSize dst_offset;
	};

	explicit AsyncDma(Device &dev);
	AsyncDma(AsyncDma &&) = delete;
	AsyncDma(const AsyncDma &) = delete;
	AsyncDma &operator=(AsyncDma &&) = delete;
	AsyncDma &operator=(const AsyncDma &) = delete;
	~AsyncDma() noexcept;

	void transfer(BufferToImageUpload upload);
	void transfer(BufferToBufferUpload upload);

	uint64_t flush();

private:
	struct TransferCommand;
	struct TransferCommandB;

	Device &m_dev;

	VkCommandPool m_cmd_pool = VK_NULL_HANDLE;
	// Cached value from device info
	const uint32_t m_dma_queue_family;
	// Index into `m_cmd_buffers`
	uint32_t m_current_cmd_buffer = 0;

	VkCommandBuffer m_cmd_buffers[MAX_CMD_BUFFERS];
	uint64_t m_cmd_submit_timelines[MAX_CMD_BUFFERS] = {};

	std::vector<std::pair<VkBuffer, VmaAllocation>> m_scratch_buffers;
	std::vector<VkImageMemoryBarrier2> m_pre_barriers;
	std::vector<TransferCommand> m_tx_cmds;
	std::vector<TransferCommandB> m_txb_cmds;
	std::vector<VkImageMemoryBarrier2> m_post_barriers;

	std::pair<VkBuffer, void *> allocateScratchBuffer(VkDeviceSize size);
};

} // namespace voxen::gfx::vk
