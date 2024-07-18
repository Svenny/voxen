#pragma once

#include <voxen/client/vulkan/buffer.hpp>
#include <voxen/client/vulkan/command_buffer.hpp>
#include <voxen/client/vulkan/command_pool.hpp>
#include <voxen/client/vulkan/config.hpp>
#include <voxen/client/vulkan/sync.hpp>

#include <voxen/common/gameview.hpp>
#include <voxen/common/world_state.hpp>

#include <extras/dyn_array.hpp>

namespace voxen::client::vulkan
{

class MainLoop {
public:
	MainLoop();
	MainLoop(MainLoop &&) = delete;
	MainLoop(const MainLoop &) = delete;
	MainLoop &operator=(MainLoop &&) = delete;
	MainLoop &operator=(const MainLoop &) = delete;
	~MainLoop() noexcept;

	void drawFrame(const WorldState &state, const GameView &view);

private:
	size_t m_frame_id = 0;
	uint64_t m_submit_timelines[Config::NUM_CPU_PENDING_FRAMES] = {};

	CommandPool m_graphics_command_pool;
	extras::dyn_array<CommandBuffer> m_graphics_command_buffers;
	FatVkBuffer m_main_scene_ubo;

	void updateMainSceneUbo(const GameView &view);
};

} // namespace voxen::client::vulkan
