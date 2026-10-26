#pragma once

#include <array>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include <voxen/os/window_events.hpp>
#include <voxen/svc/config.hpp>

#include "player_action_events.hpp"

namespace voxen::client
{

class InputEventAdapter {
public:
	static std::pair<PlayerActionEvent, bool> keyToPlayerEvent(const os::KeyEvent& event);

	static std::pair<PlayerActionEvent, bool> mouseButtonToPlayerEvent(const os::MouseButtonEvent& event);

	static std::pair<PlayerActionEvent, bool> mouseWheelToPlayerEvent(const os::MouseWheelEvent& event);

	static void init();

	static void release() noexcept;

private:
	static svc::Config::Scheme actionSettingsScheme();

	static int stringToMapkey(std::string_view s, std::tuple<int, int>* tuple_ptr,
		bool* scroll_ptr); // 0 - unknown type, 1 - keyboard, 2 - mouse button, 3 - scroll
	static std::string_view actionToString(PlayerActionEvent event);
	static PlayerActionEvent stringToAction(std::string_view s);
	static std::string keyboardKeyToString(os::Key key, int mods);
	static std::string mouseButtonToString(os::MouseButton button, int mods);
	static std::string scrollToString(double yoffset);

private:
	static std::map<std::tuple<int, int>, PlayerActionEvent> keyboardKeyToActionMap;
	static std::map<std::tuple<int, int>, PlayerActionEvent> mouseKeyToActionMap;
	static std::map<bool, PlayerActionEvent> scrollToActionMap;
	static std::string actionSettingsPath;
	static svc::Config* actionSettingsConfig;
	static svc::Config::Scheme actionSettingConfigScheme;

	static const std::array<std::pair<os::Key, std::string>, 94> KEYCODE_2_STRING;
	static const std::array<std::pair<os::MouseButton, std::string>, 5> MOUSEKEY_2_STRING;
	static const std::array<std::pair<os::KeyModBit, std::string>, 6> MODS_2_STRING;
	static const std::array<std::pair<PlayerActionEvent, std::string>, 13> PLAYERACTIONS_2_STRINGS;

	static void parseToken(std::string_view string, PlayerActionEvent event, std::string_view parameter_name,
		std::string_view default_value);
	static void parseComplexToken(std::string_view string, int& key, int& mods, int& key_type, bool& fully_parsed);

	constexpr static const std::string_view KEYS_DELIMITER = "+";
	constexpr static const std::string_view KEYS_SEQUENCE_DELIMITER = ", ";
	constexpr static const std::string_view SCROLL_UP_STRING = "ScrollUp";
	constexpr static const std::string_view SCROLL_DOWN_STRING = "ScrollDown";
};

} // namespace voxen::client
