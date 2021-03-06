#pragma once

#include <voxen/client/vulkan/common.hpp>
#include <voxen/client/vulkan/command_buffer.hpp>
#include <voxen/client/vulkan/command_pool.hpp>
#include <voxen/client/vulkan/sync.hpp>

#include <voxen/common/world_state.hpp>
#include <voxen/common/gameview.hpp>

#include <extras/dyn_array.hpp>

namespace voxen::client::vulkan
{

class MainLoop {
public:
	static inline constexpr size_t MAX_PENDING_FRAMES = 2;

	MainLoop();
	MainLoop(MainLoop &&) = delete;
	MainLoop(const MainLoop &) = delete;
	MainLoop &operator = (MainLoop &&) = delete;
	MainLoop &operator = (const MainLoop &) = delete;
	~MainLoop() noexcept;

	void drawFrame(const WorldState &state, const GameView &view);
private:
	struct PendingFrameSyncs {
		PendingFrameSyncs();
		Semaphore frame_acquired_semaphore;
		Semaphore render_done_semaphore;
		Fence render_done_fence;
	};

	size_t m_frame_id = 0;

	extras::dyn_array<VkFence> m_image_guard_fences;
	PendingFrameSyncs m_pending_frame_syncs[MAX_PENDING_FRAMES];

	CommandPool m_graphics_command_pool;
	extras::dyn_array<CommandBuffer> m_graphics_command_buffers;
};

}
