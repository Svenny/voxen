#pragma once

#include <voxen/common/gameview.hpp>
#include <voxen/os/glfw_window.hpp>
#include <voxen/svc/svc_fwd.hpp>
#include <voxen/visibility.hpp>
#include <voxen/world/world_fwd.hpp>

namespace voxen::client
{

class VOXEN_API Render {
public:
	explicit Render(os::GlfwWindow &window, svc::ServiceLocator &svc);
	Render(Render &&) = delete;
	Render(const Render &) = delete;
	Render &operator=(Render &&) = delete;
	Render &operator=(const Render &) = delete;
	~Render();

	void drawFrame(const world::State &world_state, const GameView &view);

private:
	os::GlfwWindow &m_window;
};

} // namespace voxen::client
