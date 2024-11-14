#include <voxen/gfx/vk/vk_dma_system.hpp>

#include <voxen/gfx/gfx_system.hpp>
#include <voxen/gfx/vk/vk_command_allocator.hpp>
#include <voxen/gfx/vk/vk_error.hpp>
#include <voxen/gfx/vk/vk_transient_buffer_allocator.hpp>

#include <cstring>

namespace voxen::gfx::vk
{

namespace
{

constexpr VkDeviceSize STANDARD_STAGING_ALIGNMENT = 4;

}

DmaSystem::DmaSystem(GfxSystem &gfx) : m_gfx(gfx) {}

DmaSystem::~DmaSystem() = default;

void DmaSystem::uploadToBuffer(BufferUpload upload)
{
	ensureCmdBuffer();

	auto staging = m_gfx.transientBufferAllocator()->allocate(TransientBufferAllocator::TypeUpload, upload.size,
		STANDARD_STAGING_ALIGNMENT);
	memcpy(staging.host_pointer, upload.src_data, upload.size);

	VkBufferCopy region {
		.srcOffset = staging.buffer_offset,
		.dstOffset = upload.dst_offset,
		.size = upload.size,
	};

	// TODO: should we split cmd buffers if there are too many recorded commands?
	m_gfx.device()->dt().vkCmdCopyBuffer(m_current_cmd_buf, staging.buffer, upload.dst_buffer, 1, &region);
}

uint64_t DmaSystem::flush()
{
	if (!m_current_cmd_buf) {
		return m_last_submitted_timeline;
	}

	VkResult res = m_gfx.device()->dt().vkEndCommandBuffer(m_current_cmd_buf);
	if (res != VK_SUCCESS) [[unlikely]] {
		throw VulkanException(res, "vkEndCommandBuffer");
	}

	m_last_submitted_timeline = m_gfx.device()->submitCommands({
		.queue = Device::QueueDma,
		.cmds = std::span(&m_current_cmd_buf, 1),
	});

	m_current_cmd_buf = VK_NULL_HANDLE;
	return m_last_submitted_timeline;
}

void DmaSystem::onFrameTickBegin(FrameTickId /*completed_tick*/, FrameTickId new_tick)
{
	m_current_tick_id = new_tick;
}

void DmaSystem::onFrameTickEnd(FrameTickId /*current_tick*/)
{
	flush();
}

void DmaSystem::ensureCmdBuffer()
{
	if (m_current_cmd_buf) {
		return;
	}

	m_current_cmd_buf = m_gfx.commandAllocator()->allocate(Device::QueueDma);

	VkCommandBufferBeginInfo begin_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = nullptr,
	};

	VkResult res = m_gfx.device()->dt().vkBeginCommandBuffer(m_current_cmd_buf, &begin_info);
	if (res != VK_SUCCESS) [[unlikely]] {
		throw VulkanException(res, "vkBeginCommandBuffer");
	}
}

} // namespace voxen::gfx::vk
