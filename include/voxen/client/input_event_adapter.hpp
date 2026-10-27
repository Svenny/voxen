#pragma once

#include <voxen/client/player_action_events.hpp>
#include <voxen/os/window_events.hpp>

#include <utility>

namespace voxen::client::InputEventAdapter
{

std::pair<PlayerActionEvent, bool> keyToPlayerEvent(const os::KeyEvent &event);

std::pair<PlayerActionEvent, bool> mouseButtonToPlayerEvent(const os::MouseButtonEvent &event);

std::pair<PlayerActionEvent, bool> mouseWheelToPlayerEvent(const os::MouseWheelEvent &event);

void init();

void release() noexcept;

} // namespace voxen::client::InputEventAdapter
