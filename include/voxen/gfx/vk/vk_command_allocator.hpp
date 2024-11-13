#pragma once

#include <voxen/gfx/frame_tick_id.hpp>
#include <voxen/gfx/gfx_fwd.hpp>
#include <voxen/gfx/gfx_public_consts.hpp>
#include <voxen/gfx/vk/vk_device.hpp>
#include <voxen/gfx/vk/vk_include.hpp>

#include <vector>

namespace voxen::gfx::vk
{

// This class is NOT thread-safe.
class CommandAllocator {
public:
	explicit CommandAllocator(GfxSystem &gfx);
	CommandAllocator(CommandAllocator &&) = delete;
	CommandAllocator(const CommandAllocator &) = delete;
	CommandAllocator &operator=(CommandAllocator &&) = delete;
	CommandAllocator &operator=(const CommandAllocator &) = delete;
	~CommandAllocator();

	VkCommandBuffer allocate(Device::Queue queue);

	void onFrameTickBegin(FrameTickId completed_tick, FrameTickId new_tick);
	void onFrameTickEnd(FrameTickId current_tick);

private:
	GfxSystem &m_gfx;

	std::vector<VkCommandBuffer> m_command_buffers[gfx::Consts::MAX_PENDING_FRAMES][Device::QueueCount];
	VkCommandPool m_command_pools[gfx::Consts::MAX_PENDING_FRAMES][Device::QueueCount] = {};
	FrameTickId m_tick_ids[gfx::Consts::MAX_PENDING_FRAMES];

	uint32_t m_current_set = 0;
	uint32_t m_cmd_buffer_index[Device::QueueCount] = {};
};

} // namespace voxen::gfx::vk
