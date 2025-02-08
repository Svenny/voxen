#pragma once

#include <voxen/common/gameview.hpp>
#include <voxen/os/glfw_window.hpp>
#include <voxen/svc/svc_fwd.hpp>
#include <voxen/visibility.hpp>
#include <voxen/world/world_fwd.hpp>

struct GLFWwindow;

namespace voxen::client
{

class VOXEN_API Gui {
public:
	Gui(os::GlfwWindow& window);
	~Gui() noexcept;

	void handleKey(int key, int scancode, int action, int mods);
	void handleCursor(double xpos, double ypos);
	void handleMouseKey(int button, int action, int mods);
	void handleMouseScroll(double xoffset, double yoffset);

	void init(const world::State& world_start_state);
	void update(const world::State& lastState, double dt, svc::MessageSender& msend);

	GameView& view();

private:
	GameView m_gameview;
	// TODO stack for another GUI elements
};

} // namespace voxen::client
