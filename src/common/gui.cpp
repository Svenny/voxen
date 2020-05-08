#include <voxen/common/gui.hpp>

#include <functional>
#include <GLFW/glfw3.h>

#include <voxen/common/world.hpp>
#include <voxen/util/log.hpp>

using namespace voxen;

Gui::Gui() {
}

void Gui::setWindow(Window* window){
	m_window = window;
}

void voxen::Gui::handleKey(int key, int scancode, int action, int mods)
{
	m_gameview.handleKey(key, scancode, action, mods);
}

void voxen::Gui::handleCursor(double xpos, double ypos)
{
	m_gameview.handleCursor(xpos, ypos);
}

void voxen::Gui::handleMouseKey(int button, int action, int mods)
{
	(void)button;
	(void)action;
	(void)mods;
	// GameView don't support mouse key, so don't call
	//m_gameview.handleMouseKey(button, action, mods);
}

void voxen::Gui::handleMouseScroll(double xoffset, double yoffset)
{
	m_gameview.handleMouseScroll(xoffset, yoffset);
}

GameView& Gui::view()
{
	return m_gameview;
}

void voxen::Gui::init(const voxen::World& world_start_state)
{
	m_gameview.init(world_start_state.player());
}

void voxen::Gui::update(const World& world, DebugQueueRtW& queue)
{
	m_gameview.update(world.player(), queue, world.tickId());
}
