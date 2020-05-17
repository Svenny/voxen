#pragma once

#include <voxen/common/gameview.hpp>
#include <voxen/common/world.hpp>
#include <voxen/client/window.hpp>

struct GLFWwindow;

namespace voxen::client
{

class Gui {
public:
	Gui(Window& wnd);

	void handleKey(int key, int scancode, int action, int mods);
	void handleCursor(double xpos, double ypos);
	void handleMouseKey(int button, int action, int mods);
	void handleMouseScroll(double xoffset, double yoffset);

	void init(const World& world_start_state);
	// TODO send update to world via message
	void update(const World& lastState, DebugQueueRtW& queue);

	GameView& view();
private:
	Window* m_window;
	GameView m_gameview;
	// TODO stack for another GUI elements
};

}
