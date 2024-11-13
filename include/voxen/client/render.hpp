#pragma once

#include <voxen/common/gameview.hpp>
#include <voxen/common/world_state.hpp>
#include <voxen/os/glfw_window.hpp>
#include <voxen/svc/svc_fwd.hpp>
#include <voxen/visibility.hpp>

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

	void drawFrame(const WorldState &world_state, const GameView &view);

private:
	os::GlfwWindow &m_window;
};

} // namespace voxen::client
