#include <voxen/client/gui.hpp>

#include <voxen/util/log.hpp>

#include <functional>

#include <voxen/client/input_event_adapter.hpp>

namespace voxen::client
{

Gui::Gui(Window& window): m_gameview(window) {
	window.attachGUI(*this);
}

Gui::~Gui() {
	InputEventAdapter::release();
}

void Gui::handleKey(int key, int scancode, int action, int mods)
{
	std::pair<PlayerActionEvents, bool> input = InputEventAdapter::glfwKeyboardToPlayerEvent(key, scancode, action, mods);
	if (input.first == PlayerActionEvents::None)
		return;

	m_gameview.handleEvent(input.first, input.second);
}

void Gui::handleCursor(double xpos, double ypos)
{
	m_gameview.handleCursor(xpos, ypos);
}

void Gui::handleMouseKey(int button, int action, int mods)
{
	std::pair<PlayerActionEvents, bool> input = InputEventAdapter::glfwMouseKeyToPlayerEvent(button, action, mods);
	if (input.first == PlayerActionEvents::None)
		return;

	m_gameview.handleEvent(input.first, input.second);
}

void Gui::handleMouseScroll(double xoffset, double yoffset)
{
	std::pair<PlayerActionEvents, bool> input = InputEventAdapter::glfwMouseScrollToPlayerEvent(xoffset, yoffset);
	if (input.first == PlayerActionEvents::None)
		return;

	m_gameview.handleEvent(input.first, input.second);
}

GameView& Gui::view()
{
	return m_gameview;
}

void Gui::init(const WorldState& world_start_state)
{
	m_gameview.init(world_start_state.player());
	InputEventAdapter::init();
}

void Gui::update(const WorldState& world, DebugQueueRtW& queue)
{
	m_gameview.update(world.player(), queue, world.tickId());
}

}
