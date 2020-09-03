#pragma once

#include <map>
#include <string>
#include <string_view>
#include <array>

#include <voxen/common/config.hpp>

#include "player_action_events.hpp"

namespace voxen::client {

class InputEventAdapter
{
public:
	static std::pair<PlayerActionEvents, bool> glfwKeyboardToPlayerEvent(int key, int scancode, int action, int mods);

	static std::pair<PlayerActionEvents, bool> glfwMouseKeyToPlayerEvent(int button, int action, int mods);

	static std::pair<PlayerActionEvents, bool> glfwMouseScrollToPlayerEvent(double xoffset, double yoffset);

	static void init();

	static void release();
private:
	static std::uint32_t hashGlfwKeyInput(int key, int mods);
	static std::uint32_t hashGlfwMouseKeyInput(int button, int mods);
	static std::uint32_t hashGlfwMouseScrollInput(double yoffset);

	static Config::Scheme actionSettingsScheme();

	static std::string hashToString(std::uint32_t hash);
	static std::uint32_t stringToHash(std::string_view s);
	static std::string_view actionToString(PlayerActionEvents event);
	static PlayerActionEvents stringToAction(std::string_view s);
	static std::string_view keyToString(int key);
	static std::string_view mousekeyToString(int key);

private:
	//std::map<PlayerActionEvents, std::string> actionToKeycombination;
	static std::map<std::uint32_t, PlayerActionEvents> keyToActionMap;
	static std::string actionSettingsPath;
	static Config* actionSettingsConfig;
	static Config::Scheme actionSettingConfigScheme;

	static const std::array<std::pair<int, std::string>, 94> KEYCODE_2_STRING;
	static const std::array<std::pair<int, std::string>, 8> MOUSEKEY_2_STRING;
	static const std::array<std::pair<int, std::string>, 6> MODS_2_STRING;
	static const std::array<std::pair<PlayerActionEvents, std::string>, 11> PLAYERACTIONS_2_STRINGS;

	static void parseToken(std::string_view string, PlayerActionEvents event, std::string_view parameter_name, std::string_view default_value);
	static void parseComplexToken(std::string_view string, int& key, int& mods, int& key_type, bool& fully_parsed);

	constexpr static const std::string_view KEYS_DELIMITER = "+";
	constexpr static const std::string_view KEYS_SEQUENCE_DELIMITER = ", ";
	constexpr static const std::string_view SCROLL_UP_STRING = "ScrollUp";
	constexpr static const std::string_view SCROLL_DOWN_STRING= "ScrollDown";

	constexpr static const uint32_t INVALID_HASH = 0xC0000000; // See hashGlfwKeyInput
};

}
