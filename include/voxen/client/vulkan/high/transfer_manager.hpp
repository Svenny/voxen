#pragma once

#include <voxen/gfx/vk/vk_include.hpp>

namespace voxen::client::vulkan
{

class TransferManager {
public:
	// Maximal size (in bytes) of a single upload
	constexpr static VkDeviceSize MAX_UPLOAD_SIZE = 10 * (1 << 20); // 10 MB

	TransferManager();
	TransferManager(TransferManager &&) = delete;
	TransferManager(const TransferManager &) = delete;
	TransferManager &operator=(TransferManager &&) = delete;
	TransferManager &operator=(const TransferManager &) = delete;
	~TransferManager() noexcept;

	void uploadToBuffer(VkBuffer buffer, const void *data, VkDeviceSize size);
	void uploadToBuffer(VkBuffer buffer, VkDeviceSize offset, const void *data, VkDeviceSize size);
	void ensureUploadsDone();

private:
	VkCommandBuffer m_cmd_buffer = VK_NULL_HANDLE;
	VkDeviceSize m_staging_written = 0;
};

} // namespace voxen::client::vulkan
