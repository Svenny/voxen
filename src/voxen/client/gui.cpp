#include <voxen/client/gui.hpp>

#include <voxen/client/input_event_adapter.hpp>
#include <voxen/client/player_action_events.hpp>
#include <voxen/log.hpp>
#include <voxen/world/world_state.hpp>

#include <utility>
#include <variant>

namespace voxen::client
{

Gui::Gui(os::Window &window) : m_gameview(window) {}

Gui::~Gui() noexcept
{
	InputEventAdapter::release();
}

void Gui::handleEvent(const os::WindowEvent &event)
{
	std::pair<PlayerActionEvent, bool> input(PlayerActionEvent::None, false);

	if (auto *key = std::get_if<os::KeyEvent>(&event)) {
		input = InputEventAdapter::keyToPlayerEvent(*key);
	} else if (auto *button = std::get_if<os::MouseButtonEvent>(&event)) {
		input = InputEventAdapter::mouseButtonToPlayerEvent(*button);
	} else if (auto *wheel = std::get_if<os::MouseWheelEvent>(&event)) {
		input = InputEventAdapter::mouseWheelToPlayerEvent(*wheel);
	} else if (auto *motion = std::get_if<os::MouseMotionEvent>(&event)) {
		m_gameview.handleMouseMotion(motion->delta);
	} else if (auto *resize = std::get_if<os::WindowResizeEvent>(&event)) {
		m_gameview.handleResize(resize->framebuffer_size);
	} else if (auto *text = std::get_if<os::TextInputEvent>(&event)) {
		// Nothing consumes text input yet
		Log::debug("Text input: '{}'", text->text);
	} else if (auto *edit = std::get_if<os::TextEditingEvent>(&event)) {
		Log::debug("Text composition: '{}', selected [{}, +{})", edit->text, edit->start, edit->length);
	}

	if (input.first != PlayerActionEvent::None) {
		m_gameview.handleEvent(input.first, input.second);
	}
}

GameView &Gui::view()
{
	return m_gameview;
}

void Gui::init(const world::State &world_start_state)
{
	m_gameview.init(world_start_state.player());
	InputEventAdapter::init();
}

void Gui::update(const world::State &lastState, double dt, svc::MessageSender &msend)
{
	m_gameview.update(lastState.player(), lastState.tickId(), dt, msend);
}

} // namespace voxen::client
