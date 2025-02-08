#include <voxen/client/gui.hpp>

#include <voxen/client/input_event_adapter.hpp>
#include <voxen/util/log.hpp>
#include <voxen/world/world_state.hpp>

namespace voxen::client
{

Gui::Gui(os::GlfwWindow& window) : m_gameview(window)
{
	window.attachGUI(*this);
}

Gui::~Gui() noexcept
{
	InputEventAdapter::release();
}

void Gui::handleKey(int key, int scancode, int action, int mods)
{
	std::pair<PlayerActionEvent, bool> input = InputEventAdapter::glfwKeyboardToPlayerEvent(key, scancode, action, mods);
	if (input.first == PlayerActionEvent::None) {
		return;
	}

	m_gameview.handleEvent(input.first, input.second);
}

void Gui::handleCursor(double xpos, double ypos)
{
	m_gameview.handleCursor(xpos, ypos);
}

void Gui::handleMouseKey(int button, int action, int mods)
{
	std::pair<PlayerActionEvent, bool> input = InputEventAdapter::glfwMouseKeyToPlayerEvent(button, action, mods);
	if (input.first == PlayerActionEvent::None) {
		return;
	}

	m_gameview.handleEvent(input.first, input.second);
}

void Gui::handleMouseScroll(double xoffset, double yoffset)
{
	std::pair<PlayerActionEvent, bool> input = InputEventAdapter::glfwMouseScrollToPlayerEvent(xoffset, yoffset);
	if (input.first == PlayerActionEvent::None) {
		return;
	}

	m_gameview.handleEvent(input.first, input.second);
}

GameView& Gui::view()
{
	return m_gameview;
}

void Gui::init(const world::State& world_start_state)
{
	m_gameview.init(world_start_state.player());
	InputEventAdapter::init();
}

void Gui::update(const world::State& lastState, double dt, svc::MessageQueue& mq)
{
	m_gameview.update(lastState.player(), lastState.tickId(), dt, mq);
}

} // namespace voxen::client
