#include <voxen/client/vulkan/high/transfer_manager.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/gfx/gfx_system.hpp>
#include <voxen/gfx/vk/vk_command_allocator.hpp>
#include <voxen/gfx/vk/vk_device.hpp>
#include <voxen/gfx/vk/vk_error.hpp>
#include <voxen/gfx/vk/vk_physical_device.hpp>
#include <voxen/gfx/vk/vk_transient_buffer_allocator.hpp>
#include <voxen/util/log.hpp>

namespace voxen::client::vulkan
{

TransferManager::TransferManager()
{
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

	auto &backend = Backend::backend();
	auto &device = backend.device();
	auto &cmd_alloc = *backend.gfxSystem().commandAllocator();
	auto &tsb_alloc = *backend.gfxSystem().transientBufferAllocator();

	if (m_staging_written == 0) {
		m_cmd_buffer = cmd_alloc.allocate(gfx::vk::Device::QueueDma);

		VkCommandBufferBeginInfo begin_info {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = nullptr,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			.pInheritanceInfo = nullptr,
		};

		VkResult res = device.dt().vkBeginCommandBuffer(m_cmd_buffer, &begin_info);
		if (res != VK_SUCCESS) [[unlikely]] {
			throw gfx::vk::VulkanException(res, "vkBeginCommandBuffer");
		}
	}

	auto alloc = tsb_alloc.allocate(gfx::vk::TransientBufferAllocator::TypeUpload, size, 4);
	assert(alloc.host_pointer);
	memcpy(alloc.host_pointer, data, size);

	VkBufferCopy copy {
		.srcOffset = alloc.buffer_offset,
		.dstOffset = offset,
		.size = size,
	};

	device.dt().vkCmdCopyBuffer(m_cmd_buffer, alloc.buffer, buffer, 1, &copy);

	m_staging_written += size;
}

void TransferManager::ensureUploadsDone()
{
	if (m_staging_written == 0) {
		return;
	}

	auto &backend = Backend::backend();
	auto &device = backend.device();

	VkResult res = device.dt().vkEndCommandBuffer(m_cmd_buffer);
	if (res != VK_SUCCESS) [[unlikely]] {
		throw gfx::vk::VulkanException(res, "vkEndCommandBuffer");
	}

	uint64_t timeline = device.submitCommands({
		.queue = gfx::vk::Device::QueueDma,
		.cmds = std::span(&m_cmd_buffer, 1),
	});
	// TODO: do it asynchronously
	device.waitForTimeline(gfx::vk::Device::QueueDma, timeline);

	m_cmd_buffer = VK_NULL_HANDLE;
	m_staging_written = 0;
}

} // namespace voxen::client::vulkan
