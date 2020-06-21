#include <voxen/client/gui.hpp>

#include <functional>
#include <GLFW/glfw3.h>

#include <voxen/common/world.hpp>
#include <voxen/util/log.hpp>

namespace voxen::client
{

Gui::Gui(Window& window): m_window(&window), m_gameview(window) {
	window.attachGUI(*this);
}

void Gui::handleKey(int key, int scancode, int action, int mods)
{
	m_gameview.handleKey(key, scancode, action, mods);
}

void Gui::handleCursor(double xpos, double ypos)
{
	m_gameview.handleCursor(xpos, ypos);
}

void Gui::handleMouseKey(int button, int action, int mods)
{
	m_gameview.handleMouseKey(button, action, mods);
}

void Gui::handleMouseScroll(double xoffset, double yoffset)
{
	m_gameview.handleMouseScroll(xoffset, yoffset);
}

GameView& Gui::view()
{
	return m_gameview;
}

void Gui::init(const World& world_start_state)
{
	m_gameview.init(world_start_state.player());
}

void Gui::update(const World& world, DebugQueueRtW& queue)
{
	m_gameview.update(world.player(), queue, world.tickId());
}

}