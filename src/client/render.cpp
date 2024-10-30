#include <voxen/client/render.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/util/error_condition.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

namespace voxen::client
{

Render::Render(os::GlfwWindow &window) : m_window(window)
{
	if (!vulkan::Backend::backend().start(m_window)) {
		Log::error("Render subsystem couldn't launch");
		throw Exception::fromError(VoxenErrc::GfxFailure, "failed to start render subsystem");
	}
}

Render::~Render()
{
	vulkan::Backend::backend().stop();
}

void Render::drawFrame(const WorldState &world_state, const GameView &view)
{
	auto &backend = vulkan::Backend::backend();
	if (!backend.drawFrame(world_state, view)) {
		auto state = backend.state();

		if (state == vulkan::Backend::State::Broken) {
			Log::error("Render subsystem encountered a non-recoverable error, shutting down");
			throw Exception::fromError(VoxenErrc::GfxFailure, "render subsystem failure");
		} else {
			// This branch must be unreachable
			assert(false);
		}
	}
}

} // namespace voxen::client
