#include <voxen/client/render.hpp>

#include <voxen/client/vulkan/high/main_loop.hpp>
#include <voxen/client/vulkan/backend.hpp>

#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

namespace voxen::client
{

Render::Render(Window &window)
	: m_window(window)
{
	if (!vulkan::Backend::backend().start(m_window)) {
		Log::error("Render subsystem couldn't launch");
		throw MessageException("failed to start render subsystem");
	}
}

Render::~Render()
{
	vulkan::Backend::backend().stop();
}

void Render::drawFrame(const WorldState &state, const GameView &view)
{
	auto &backend = vulkan::Backend::backend();
	if (!backend.drawFrame(state, view)) {
		auto state = backend.state();

		if (state == vulkan::Backend::State::Broken) {
			Log::error("Render subsystem encountered a non-recoverable error, shutting down");
			throw MessageException("render subsystem failure");
		}
		else if (state == vulkan::Backend::State::SurfaceLost) {
			Log::info("Trying to recreate surface...");
			if (!backend.recreateSurface(m_window)) {
				Log::error("Surface recreation failed, shutting down");
				throw MessageException("failed to recreate surface");
			}
		}
		else if (state == vulkan::Backend::State::SwapchainOutOfDate) {
			Log::info("Trying to recreate swapchain...");
			if (!backend.recreateSwapchain(m_window)) {
				Log::error("Swapchain recreation failed, shutting down");
				throw MessageException("failed to recreate swapchain");
			}
		}
		else {
			// This branch must be unreachable
			assert(false);
		}
	}
}

}
