#pragma once

#include <voxen/gfx/frame_tick_id.hpp>
#include <voxen/gfx/gfx_fwd.hpp>
#include <voxen/gfx/vk/vk_include.hpp>

namespace voxen::gfx::vk
{

// This class is NOT thread-safe.
class DmaSystem {
public:
	struct BufferUpload {
		const void *src_data = nullptr;
		VkBuffer dst_buffer = VK_NULL_HANDLE;
		VkDeviceSize dst_offset = 0;
		VkDeviceSize size = 0;
	};

	struct BufferCopy {
		VkBuffer src_buffer = VK_NULL_HANDLE;
		VkBuffer dst_buffer = VK_NULL_HANDLE;
		VkDeviceSize src_offset = 0;
		VkDeviceSize dst_offset = 0;
		VkDeviceSize size = 0;
	};

	explicit DmaSystem(GfxSystem &gfx);
	DmaSystem(DmaSystem &&) = delete;
	DmaSystem(const DmaSystem &) = delete;
	DmaSystem &operator=(DmaSystem &&) = delete;
	DmaSystem &operator=(const DmaSystem &) = delete;
	~DmaSystem();

	void uploadToBuffer(BufferUpload upload);
	void copyBufferToBuffer(BufferCopy copy);

	uint64_t flush();

	void onFrameTickBegin(FrameTickId completed_tick, FrameTickId new_tick);
	void onFrameTickEnd(FrameTickId current_tick);

private:
	GfxSystem &m_gfx;

	FrameTickId m_current_tick_id = FrameTickId::INVALID;
	VkCommandBuffer m_current_cmd_buf = VK_NULL_HANDLE;
	uint64_t m_last_submitted_timeline = 0;

	void ensureCmdBuffer();
};

} // namespace voxen::gfx::vk
