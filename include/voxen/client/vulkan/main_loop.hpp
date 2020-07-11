#pragma once

#include <voxen/client/vulkan/common.hpp>
#include <voxen/client/vulkan/command_buffer.hpp>
#include <voxen/client/vulkan/command_pool.hpp>
#include <voxen/client/vulkan/sync.hpp>

#include <extras/dyn_array.hpp>

namespace voxen::client
{

class VulkanMainLoop {
public:
	static inline constexpr size_t MAX_PENDING_FRAMES = 2;

	VulkanMainLoop();
	VulkanMainLoop(VulkanMainLoop &&) = delete;
	VulkanMainLoop(const VulkanMainLoop &) = delete;
	VulkanMainLoop &operator = (VulkanMainLoop &&) = delete;
	VulkanMainLoop &operator = (const VulkanMainLoop &) = delete;
	~VulkanMainLoop() noexcept;

	void drawFrame();
private:
	struct PendingFrameSyncs {
		PendingFrameSyncs();
		VulkanSemaphore frame_acquired_semaphore;
		VulkanSemaphore render_done_semaphore;
		VulkanFence render_done_fence;
	};

	size_t m_frame_id = 0;

	extras::dyn_array<VkFence> m_image_guard_fences;
	PendingFrameSyncs m_pending_frame_syncs[MAX_PENDING_FRAMES];

	VulkanCommandPool m_graphics_command_pool;
	extras::dyn_array<VulkanCommandBuffer> m_graphics_command_buffers;
};

}
