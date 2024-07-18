#pragma once

#include <voxen/common/gameview.hpp>
#include <voxen/common/world_state.hpp>
#include <voxen/gfx/vk/frame_context.hpp>

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
	gfx::vk::FrameContextRing m_fctx_ring;
	VkDescriptorSet createMainSceneDset(const GameView &view);
};

} // namespace voxen::client::vulkan
