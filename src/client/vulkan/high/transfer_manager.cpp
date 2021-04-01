#include <voxen/client/vulkan/high/transfer_manager.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>
#include <voxen/client/vulkan/physical_device.hpp>

#include <voxen/util/log.hpp>

namespace voxen::client::vulkan
{

TransferManager::TransferManager()
	: m_command_pool(Backend::backend().physicalDevice().transferQueueFamily()),
	m_command_buffers(m_command_pool.allocateCommandBuffers(1)),
	m_staging_buffer(createStagingBuffer())
{
	auto &backend = Backend::backend();
	VkDevice device = *backend.device();
	VkDeviceMemory memory = m_staging_buffer.allocation().handle();
	VkResult result = backend.vkMapMemory(device, memory, 0, VK_WHOLE_SIZE, 0, &m_staging_mapped_data);
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkMapMemory");

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
	if (size == 0) {
		Log::warn("Empty upload requested");
		return;
	}

	// TODO: handle the case when no staging buffer is needed (integrated GPUs)
	if (size > MAX_UPLOAD_SIZE) {
		Log::error("Upload size is {} bytes which exceeds the limit ({} bytes)", size, MAX_UPLOAD_SIZE);
		throw MessageException("upload size limit exceeded");
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
	copy.dstOffset = 0;
	copy.size = size;
	// TODO: handle the case when transfer and target queues are different
	backend.vkCmdCopyBuffer(m_command_buffers[0], m_staging_buffer, buffer, 1, &copy);

	m_staging_written += size;
}

void TransferManager::ensureUploadsDone()
{
	if (m_staging_written == 0)
		return;
	// TODO: call vkFlushMappedMemoryRanges if allocation is in non-coherent memory
	m_command_buffers[0].end();
	m_staging_written = 0;

	auto &backend = Backend::backend();
	auto &device = *backend.device();

	VkSubmitInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	info.commandBufferCount = 1;
	VkCommandBuffer cmd_buf = m_command_buffers[0];
	info.pCommandBuffers = &cmd_buf;
	backend.vkQueueSubmit(device.transferQueue(), 1, &info, VK_NULL_HANDLE);
	// TODO: use a proper synchronisation (fence?)
	device.waitIdle();
}

Buffer TransferManager::createStagingBuffer()
{
	VkBufferCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	info.size = MAX_UPLOAD_SIZE;
	info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	return Buffer(info, Buffer::Usage::Staging);
}

}
