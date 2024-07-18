#include <voxen/client/vulkan/high/transfer_manager.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/gfx/vk/vk_device.hpp>
#include <voxen/gfx/vk/vk_physical_device.hpp>

#include <voxen/util/log.hpp>

namespace voxen::client::vulkan
{

TransferManager::TransferManager()
	: m_command_pool(Backend::backend().device().info().dma_queue_family)
	, m_command_buffers(m_command_pool.allocateCommandBuffers(1))
	, m_staging_buffer(createStagingBuffer())
{
	m_staging_mapped_data = m_staging_buffer.hostPointer();
	// Asserting because buffer is allocated with `Upload` use case which guarantees host visibility
	assert(m_staging_mapped_data);

	Log::debug("TransferManager created successfully");
}

TransferManager::~TransferManager() noexcept
{
	Log::debug("Destroying TransferManager");
	// TODO: flush/invalidate mapped memory ranges?
	// TODO: unmap memory? (unmap is done implicitly when deleting memory object)
}

void TransferManager::uploadToBuffer(VkBuffer buffer, const void *data, VkDeviceSize size)
{
	uploadToBuffer(buffer, 0, data, size);
}

void TransferManager::uploadToBuffer(VkBuffer buffer, VkDeviceSize offset, const void *data, VkDeviceSize size)
{
	if (size == 0) [[unlikely]] {
		Log::warn("Empty upload requested");
		return;
	}

	// TODO: handle the case when no staging buffer is needed (iGPU/UMA or PCI BAR mappings)

	// Split large requests into smaller ones, simplifying logic after this loop
	while (size > MAX_UPLOAD_SIZE) {
		uploadToBuffer(buffer, offset, data, MAX_UPLOAD_SIZE);

		offset += MAX_UPLOAD_SIZE;
		data = reinterpret_cast<const std::byte *>(data) + MAX_UPLOAD_SIZE;
		size -= MAX_UPLOAD_SIZE;
	}

	if (m_staging_written + size > MAX_UPLOAD_SIZE) {
		// This write will overflow the buffer, so flush it
		ensureUploadsDone();
	}

	if (m_staging_written == 0) {
		// If nothing was written so far, the command buffer is guaranteed to be not started
		VkCommandBufferBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		m_command_buffers[0].begin(info);
	}

	uintptr_t ptr = uintptr_t(m_staging_mapped_data) + uintptr_t(m_staging_written);
	memcpy(reinterpret_cast<void *>(ptr), data, size);

	auto &backend = Backend::backend();
	VkBufferCopy copy = {};
	copy.srcOffset = m_staging_written;
	copy.dstOffset = offset;
	copy.size = size;
	// TODO: handle the case when transfer and target queues are different
	backend.vkCmdCopyBuffer(m_command_buffers[0], m_staging_buffer, buffer, 1, &copy);

	m_staging_written += size;
}

void TransferManager::ensureUploadsDone()
{
	if (m_staging_written == 0) {
		return;
	}
	// TODO: call vkFlushMappedMemoryRanges if allocation is in non-coherent memory
	m_command_buffers[0].end();
	m_staging_written = 0;

	auto &backend = Backend::backend();
	auto &device = backend.device();

	VkCommandBuffer cmd_buf = m_command_buffers[0];

	uint64_t timeline = device.submitCommands({
		.queue = gfx::vk::Device::QueueDma,
		.cmds = std::span(&cmd_buf, 1),
	});
	// TODO: do it asynchronously
	device.waitForTimeline(gfx::vk::Device::QueueDma, timeline);
}

FatVkBuffer TransferManager::createStagingBuffer()
{
	VkBufferCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	info.size = MAX_UPLOAD_SIZE;
	info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	return FatVkBuffer(info, FatVkBuffer::Usage::Staging);
}

} // namespace voxen::client::vulkan
