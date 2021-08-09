#pragma once

#include <voxen/client/vulkan/common.hpp>
#include <voxen/client/vulkan/buffer.hpp>
#include <voxen/client/vulkan/command_pool.hpp>

namespace voxen::client::vulkan
{

class TransferManager {
public:
	// Maximal size (in bytes) of a single upload
	constexpr static VkDeviceSize MAX_UPLOAD_SIZE = 10 * (1 << 20); // 10 MB

	TransferManager();
	TransferManager(TransferManager &&) = delete;
	TransferManager(const TransferManager &) = delete;
	TransferManager &operator = (TransferManager &&) = delete;
	TransferManager &operator = (const TransferManager &) = delete;
	~TransferManager() noexcept;

	void uploadToBuffer(VkBuffer buffer, const void *data, VkDeviceSize size);
	void uploadToBuffer(VkBuffer buffer, VkDeviceSize offset, const void *data, VkDeviceSize size);
	void ensureUploadsDone();
private:
	CommandPool m_command_pool;
	extras::dyn_array<CommandBuffer> m_command_buffers;
	FatVkBuffer m_staging_buffer;
	void *m_staging_mapped_data = nullptr;
	VkDeviceSize m_staging_written = 0;

	FatVkBuffer createStagingBuffer();
};

}
