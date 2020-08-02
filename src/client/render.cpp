#include <voxen/client/render.hpp>

#include <voxen/client/vulkan/high/main_loop.hpp>
#include <voxen/client/vulkan/backend.hpp>

#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

namespace voxen::client
{

Render::Render(Window &window) {
	if (!VulkanBackend::backend().start(window)) {
		Log::error("Render subsystem couldn't launch");
		throw MessageException("failed to start render subsystem");
	}
}

Render::~Render() {
	VulkanBackend::backend().stop();
}

void Render::drawFrame(const World &state, const GameView &view) {
	VulkanBackend::backend().mainLoop()->drawFrame(state, view);
}

}
