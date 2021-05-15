#include <voxen/client/input_event_adapter.hpp>
#include <voxen/common/filemanager.hpp>

#include <cassert>
#include <voxen/util/log.hpp>

#include <GLFW/glfw3.h>
#include <extras/string_utils.hpp>

using std::tuple;
using std::make_pair;
using std::string;
using std::string_view;

namespace voxen::client {

const std::array<std::pair<int, string>, 94> InputEventAdapter::KEYCODE_2_STRING = {
	make_pair(GLFW_KEY_SPACE, "SPACE"),
	make_pair(GLFW_KEY_APOSTROPHE, "APOSTROPHE"), /* ' */
	make_pair(GLFW_KEY_COMMA, "COMMA"), /* , */
	make_pair(GLFW_KEY_MINUS, "MINUS"), /* - */
	make_pair(GLFW_KEY_PERIOD, "PERIOD"), /* . */
	make_pair(GLFW_KEY_SLASH, "SLASH"), /* / */
	make_pair(GLFW_KEY_0, "0"),
	make_pair(GLFW_KEY_1, "1"),
	make_pair(GLFW_KEY_2, "2"),
	make_pair(GLFW_KEY_3, "3"),
	make_pair(GLFW_KEY_4, "4"),
	make_pair(GLFW_KEY_5, "5"),
	make_pair(GLFW_KEY_6, "6"),
	make_pair(GLFW_KEY_7, "7"),
	make_pair(GLFW_KEY_8, "8"),
	make_pair(GLFW_KEY_9, "9"),
	make_pair(GLFW_KEY_SEMICOLON, "SEMICOLON"), /* ; */
	make_pair(GLFW_KEY_EQUAL, "EQUAL"), /* = */
	make_pair(GLFW_KEY_A, "A"),
	make_pair(GLFW_KEY_B, "B"),
	make_pair(GLFW_KEY_C, "C"),
	make_pair(GLFW_KEY_D, "D"),
	make_pair(GLFW_KEY_E, "E"),
	make_pair(GLFW_KEY_F, "F"),
	make_pair(GLFW_KEY_G, "G"),
	make_pair(GLFW_KEY_H, "H"),
	make_pair(GLFW_KEY_I, "I"),
	make_pair(GLFW_KEY_J, "J"),
	make_pair(GLFW_KEY_K, "K"),
	make_pair(GLFW_KEY_L, "L"),
	make_pair(GLFW_KEY_M, "M"),
	make_pair(GLFW_KEY_N, "N"),
	make_pair(GLFW_KEY_O, "O"),
	make_pair(GLFW_KEY_P, "P"),
	make_pair(GLFW_KEY_Q, "Q"),
	make_pair(GLFW_KEY_R, "R"),
	make_pair(GLFW_KEY_S, "S"),
	make_pair(GLFW_KEY_T, "T"),
	make_pair(GLFW_KEY_U, "U"),
	make_pair(GLFW_KEY_V, "V"),
	make_pair(GLFW_KEY_W, "W"),
	make_pair(GLFW_KEY_X, "X"),
	make_pair(GLFW_KEY_Y, "Y"),
	make_pair(GLFW_KEY_Z, "Z"),
	make_pair(GLFW_KEY_LEFT_BRACKET, "["), /* [ */
	make_pair(GLFW_KEY_BACKSLASH, "\\"), /* \ */
	make_pair(GLFW_KEY_RIGHT_BRACKET, "]"), /* ] */
	make_pair(GLFW_KEY_GRAVE_ACCENT, "`"), /* ` */
	make_pair(GLFW_KEY_ESCAPE, "ESCAPE"),
	make_pair(GLFW_KEY_ENTER, "ENTER"),
	make_pair(GLFW_KEY_TAB, "TAB"),
	make_pair(GLFW_KEY_BACKSPACE, "BACKSPACE"),
	make_pair(GLFW_KEY_INSERT, "INSERT"),
	make_pair(GLFW_KEY_DELETE, "DELETE"),
	make_pair(GLFW_KEY_RIGHT, "RIGHT"),
	make_pair(GLFW_KEY_LEFT, "LEFT"),
	make_pair(GLFW_KEY_DOWN, "DOWN"),
	make_pair(GLFW_KEY_UP, "UP"),
	make_pair(GLFW_KEY_PAGE_UP, "PAGE_UP"),
	make_pair(GLFW_KEY_PAGE_DOWN, "PAGE_DOWN"),
	make_pair(GLFW_KEY_HOME, "HOME"),
	make_pair(GLFW_KEY_END, "END"),
	make_pair(GLFW_KEY_PRINT_SCREEN, "PRINTSCREEN"),
	make_pair(GLFW_KEY_PAUSE, "PAUSE"),
	make_pair(GLFW_KEY_F1, "F1"),
	make_pair(GLFW_KEY_F2, "F2"),
	make_pair(GLFW_KEY_F3, "F3"),
	make_pair(GLFW_KEY_F4, "F4"),
	make_pair(GLFW_KEY_F5, "F5"),
	make_pair(GLFW_KEY_F6, "F6"),
	make_pair(GLFW_KEY_F7, "F7"),
	make_pair(GLFW_KEY_F8, "F8"),
	make_pair(GLFW_KEY_F9, "F9"),
	make_pair(GLFW_KEY_F10, "F10"),
	make_pair(GLFW_KEY_F11, "F11"),
	make_pair(GLFW_KEY_F12, "F12"),
	make_pair(GLFW_KEY_KP_0, "NUMPAD_0"),
	make_pair(GLFW_KEY_KP_1, "NUMPAD_1"),
	make_pair(GLFW_KEY_KP_2, "NUMPAD_2"),
	make_pair(GLFW_KEY_KP_3, "NUMPAD_3"),
	make_pair(GLFW_KEY_KP_4, "NUMPAD_4"),
	make_pair(GLFW_KEY_KP_5, "NUMPAD_5"),
	make_pair(GLFW_KEY_KP_6, "NUMPAD_6"),
	make_pair(GLFW_KEY_KP_7, "NUMPAD_7"),
	make_pair(GLFW_KEY_KP_8, "NUMPAD_8"),
	make_pair(GLFW_KEY_KP_9, "NUMPAD_9"),
	make_pair(GLFW_KEY_KP_DECIMAL, "NUMPAD_DECIMAL"),
	make_pair(GLFW_KEY_KP_DIVIDE, "NUMPAD_DIVIDE"),
	make_pair(GLFW_KEY_KP_MULTIPLY, "NUMPAD_MULTIPLY"),
	make_pair(GLFW_KEY_KP_SUBTRACT, "NUMPAD_SUBTRACT"),
	make_pair(GLFW_KEY_KP_ADD, "NUMPAD_ADD"),
	make_pair(GLFW_KEY_KP_ENTER, "NUMPAD_ENTER"),
	make_pair(GLFW_KEY_KP_EQUAL, "NUMPAD_EQUAL"),
	make_pair(GLFW_KEY_MENU, "MENU")

	// NOTE(sirgienko) Disable due strange logic press/release logic from GLFW
	// For example, you press LCONTROL
	// Press event: LCONTROL
	// Release event: LCONTROL+CONTROL
	// So, mods setted only on release, which broke logic for some actions
	/*
	make_pair(GLFW_KEY_CAPS_LOCK, "CAPS_LOCK"),
	make_pair(GLFW_KEY_SCROLL_LOCK, "SCROLL_LOCK"),
	make_pair(GLFW_KEY_NUM_LOCK, "NUM_LOCK"),
	make_pair(GLFW_KEY_LEFT_SHIFT, "LSHIFT"),
	make_pair(GLFW_KEY_LEFT_CONTROL, "LCONTROL"),
	make_pair(GLFW_KEY_LEFT_ALT, "LALT"),
	make_pair(GLFW_KEY_LEFT_SUPER, "LSUPER"),
	make_pair(GLFW_KEY_RIGHT_SHIFT, "RSHIFT"),
	make_pair(GLFW_KEY_RIGHT_CONTROL, "RCONTROL"),
	make_pair(GLFW_KEY_RIGHT_ALT, "RALT"),
	make_pair(GLFW_KEY_RIGHT_SUPER, "RSUPER"),
	*/
};

const std::array<std::pair<int, string>, 6> InputEventAdapter::MODS_2_STRING = {
	make_pair(GLFW_MOD_SHIFT, "Shift"),
	make_pair(GLFW_MOD_CONTROL, "Control"),
	make_pair(GLFW_MOD_ALT, "Alt"),
	make_pair(GLFW_MOD_SUPER, "Super"),
	make_pair(GLFW_MOD_CAPS_LOCK, "CapsLock"),
	make_pair(GLFW_MOD_NUM_LOCK, "Numlock")
};

const std::array<std::pair<PlayerActionEvent, string>, 11> InputEventAdapter::PLAYERACTIONS_2_STRINGS= {
    make_pair(PlayerActionEvent::MoveForward, "move forward"),
    make_pair(PlayerActionEvent::MoveBackward, "move backward"),
    make_pair(PlayerActionEvent::MoveUp, "move up"),
    make_pair(PlayerActionEvent::MoveDown, "stop move down"),
    make_pair(PlayerActionEvent::MoveLeft, "move left"),
    make_pair(PlayerActionEvent::MoveRight, "move right"),
    make_pair(PlayerActionEvent::RollLeft, "roll left"),
    make_pair(PlayerActionEvent::RollRight, "roll right"),
    make_pair(PlayerActionEvent::PauseGame, "pause game"),
    make_pair(PlayerActionEvent::IncreaseSpeed, "increase speed"),
    make_pair(PlayerActionEvent::DecreaseSpeed, "decrease speed")
};

const std::array<std::pair<int, string>, 8> InputEventAdapter::MOUSEKEY_2_STRING = {
	make_pair(GLFW_MOUSE_BUTTON_LEFT, "MOUSE_LEFT"),
	make_pair(GLFW_MOUSE_BUTTON_RIGHT, "MOUSE_RIGHT"),
	make_pair(GLFW_MOUSE_BUTTON_MIDDLE, "MOUSE_MIDDLE"),
	make_pair(GLFW_MOUSE_BUTTON_4, "MOUSE_4"),
	make_pair(GLFW_MOUSE_BUTTON_5, "MOUSE_5"),
	make_pair(GLFW_MOUSE_BUTTON_6, "MOUSE_6"),
	make_pair(GLFW_MOUSE_BUTTON_7, "MOUSE_7"),
	make_pair(GLFW_MOUSE_BUTTON_8, "MOUSE_8")
};

std::map<tuple<int, int>, PlayerActionEvent> InputEventAdapter::keyboardKeyToActionMap;
std::map<tuple<int, int>, PlayerActionEvent> InputEventAdapter::mouseKeyToActionMap;
std::map<bool, PlayerActionEvent> InputEventAdapter::scrollToActionMap;
string InputEventAdapter::actionSettingsPath = "configs/action_settings.ini";
Config::Scheme InputEventAdapter::actionSettingConfigScheme = InputEventAdapter::actionSettingsScheme();
Config* InputEventAdapter::actionSettingsConfig = nullptr;

std::pair<PlayerActionEvent, bool> InputEventAdapter::glfwKeyboardToPlayerEvent(int key, int scancode, int action, int mods) {
	// `action` have three state: press, release, repeat. We support for actions only press/release logic on this moment, so
	// so pass only values with two state: press/repreate - true and release - false
	// More info: https://www.glfw.org/docs/3.3.2/group__input.html#ga2485743d0b59df3791c45951c4195265
	(void)scancode;

	auto search = keyboardKeyToActionMap.find(tuple<int, int>(key, mods));
	if (search != keyboardKeyToActionMap.end())
		return make_pair(search->second, action != GLFW_RELEASE);
	else
		return make_pair(PlayerActionEvent::None, true);
}

std::pair<PlayerActionEvent, bool> InputEventAdapter::glfwMouseKeyToPlayerEvent(int button, int action, int mods) {
	auto search = mouseKeyToActionMap.find(tuple<int, int>(button, mods));
	if (search != mouseKeyToActionMap.end())
		return make_pair(search->second, action != GLFW_RELEASE);
	else
		return make_pair(PlayerActionEvent::None, true);
}

std::pair<PlayerActionEvent, bool> InputEventAdapter::glfwMouseScrollToPlayerEvent(double xoffset, double yoffset) {
	(void)xoffset;

	auto search = scrollToActionMap.find(yoffset >= 0);
    if (search != scrollToActionMap.end())
		return make_pair(search->second, true);
	else
		return make_pair(PlayerActionEvent::None, true);
}

void InputEventAdapter::init() {
	assert(actionSettingsConfig == nullptr);
	actionSettingsConfig = new Config(FileManager::userDataPath() / actionSettingsPath, actionSettingConfigScheme);

	std::function<void(string_view, voxen::client::PlayerActionEvent, string_view, string_view)> functor = InputEventAdapter::parseToken;
	for (const Config::SchemeEntry& entry : actionSettingConfigScheme) {
		const string& value_string = actionSettingsConfig->optionString(entry.section, entry.parameter_name);

		string_view value(value_string);
		PlayerActionEvent event = stringToAction(entry.parameter_name);

		const string& default_value = std::get<string>(entry.default_value);
		string_view parameter_name(entry.parameter_name);

		if (event != PlayerActionEvent::None) {
			if (value.find(KEYS_SEQUENCE_DELIMITER) == string::npos)
				parseToken(value, event, parameter_name, default_value);
			else
				string_split_apply(value, KEYS_SEQUENCE_DELIMITER, [event, &default_value, parameter_name](string_view s){
					parseToken(s, event, parameter_name, default_value);
				});
		}
		else
			Log::warn("Unknown action value while parsing key action file: \"{}\" key with \"{}\" value", entry.parameter_name, value_string);
	}
}

void InputEventAdapter::release() noexcept {
	delete actionSettingsConfig;
	actionSettingsConfig = nullptr;
}

Config::Scheme InputEventAdapter::actionSettingsScheme() {
	static const char* player_section = "player actions";
	Config::Scheme s;

	s.push_back({player_section, string(actionToString(PlayerActionEvent::MoveForward)), "Fly forward", keyboardKeyToString(GLFW_KEY_W, 0)});
	s.push_back({player_section, string(actionToString(PlayerActionEvent::MoveRight)), "Fly right", keyboardKeyToString(GLFW_KEY_D, 0)});
	s.push_back({player_section, string(actionToString(PlayerActionEvent::MoveLeft)), "Fly left", keyboardKeyToString(GLFW_KEY_A, 0)});
	s.push_back({player_section, string(actionToString(PlayerActionEvent::MoveBackward)), "Fly backward", keyboardKeyToString(GLFW_KEY_S, 0)});
	s.push_back({player_section, string(actionToString(PlayerActionEvent::MoveDown)), "Fly down", keyboardKeyToString(GLFW_KEY_C, 0)});
	s.push_back({player_section, string(actionToString(PlayerActionEvent::MoveUp)), "Fly up", keyboardKeyToString(GLFW_KEY_SPACE, 0)});
	s.push_back({player_section, string(actionToString(PlayerActionEvent::RollLeft)), "Roll counterclockwise", keyboardKeyToString(GLFW_KEY_Q, 0)});
	s.push_back({player_section, string(actionToString(PlayerActionEvent::RollRight)), "Rool clockwise", keyboardKeyToString(GLFW_KEY_E, 0)});
	s.push_back({
		player_section, string(actionToString(PlayerActionEvent::PauseGame)), "Pause movement and release cursor",
			mouseButtonToString(GLFW_MOUSE_BUTTON_LEFT, 0) + string(KEYS_SEQUENCE_DELIMITER) + keyboardKeyToString(GLFW_KEY_ESCAPE, 0)
	});

	s.push_back({player_section, string(actionToString(PlayerActionEvent::IncreaseSpeed)), "Incrase movement speed", scrollToString(1.0)});
	s.push_back({player_section, string(actionToString(PlayerActionEvent::DecreaseSpeed)), "Decrease movement speed", scrollToString(-1.0)});

	return s;
}

string_view InputEventAdapter::actionToString(PlayerActionEvent event) {
	for (auto it = PLAYERACTIONS_2_STRINGS.begin(); it != PLAYERACTIONS_2_STRINGS.end(); it++) {
		if (it->first == event)
			return string_view(it->second);
	}
	return string_view();
}

PlayerActionEvent InputEventAdapter::stringToAction(string_view s) {
for (auto it = PLAYERACTIONS_2_STRINGS.begin(); it != PLAYERACTIONS_2_STRINGS.end(); it++) {
		if (it->second == s)
			return it->first;
	}
	return PlayerActionEvent::None;
}

string InputEventAdapter::keyboardKeyToString(int key, int mods) {
	string result;

	for (auto it = KEYCODE_2_STRING.begin(); it != KEYCODE_2_STRING.end(); it++) {
		if (it->first == key)
			result = it->second;
	}

	for (auto it = MODS_2_STRING.begin(); it != MODS_2_STRING.end(); it++)
		if (mods & it->first)
			result += string(KEYS_DELIMITER)+it->second;

	return result;
}

std::string InputEventAdapter::scrollToString(double yoffset) {
	if (yoffset >= 0)
		return string(SCROLL_UP_STRING);
	else
		return string(SCROLL_DOWN_STRING);
}

string InputEventAdapter::mouseButtonToString(int button, int mods) {
	string result;

	for (auto it = MOUSEKEY_2_STRING.begin(); it != MOUSEKEY_2_STRING.end(); it++) {
		if (it->first == button)
			result = it->second;
	}

	for (auto it = MODS_2_STRING.begin(); it != MODS_2_STRING.end(); it++)
		if (mods & it->first)
			result += string(KEYS_DELIMITER)+it->second;

	return result;
}

int InputEventAdapter::stringToMapkey(string_view s, tuple<int, int>* tuple_ptr, bool* scroll_ptr) {
	if (s.find(KEYS_DELIMITER) == string::npos) {
		// Simple form, just only key without any modifiers
		for(const std::pair<int, string>& pair : KEYCODE_2_STRING)
			if (pair.second == s) {
				*tuple_ptr = tuple<int, int>(pair.first, 0);
				return 1;
			}
		for(const std::pair<int, string>& pair : MOUSEKEY_2_STRING)
			if (pair.second == s) {
				*tuple_ptr = tuple<int, int>(pair.first, 0);
				return 2;
			}
		if (SCROLL_UP_STRING == s) {
			*scroll_ptr = true;
			return 3;
		}
		else if (SCROLL_DOWN_STRING == s) {
			*scroll_ptr = false;
			return 3;
		}
		else {
			return 0;
		}
	}
	else {
		int key = 0;
		int mods = 0;
		int key_type = 0; //0 - unknown, 1 - key button, 2 - mouse button
		bool fully_parsed = true;

		string_split_apply(s, KEYS_DELIMITER, [&key, &mods, &key_type, &fully_parsed](string_view subtoken){
			parseComplexToken(subtoken, key, mods, key_type, fully_parsed);
		});

		if (fully_parsed == false)
			return 0;
		else {
			if (key_type == 0)
				return 0;
			else if (key_type == 1) {
				*tuple_ptr = tuple<int, int>(key, mods);
				return 1;
			}
			else if (key_type == 2) {
				*tuple_ptr = tuple<int, int>(key, mods);
				return 2;
			}
			else {
				assert(false);
				return 0;
			}
		}
	}
}

void InputEventAdapter::parseToken(string_view s, PlayerActionEvent event, string_view parameter_name, string_view default_value) {
	tuple<int, int> keys_with_mods;
	bool scroll;

	int type = stringToMapkey(s, &keys_with_mods, &scroll);
	if (type != 0)
		if (type == 1)
			keyboardKeyToActionMap[keys_with_mods] = event;
		else if (type == 2)
			mouseKeyToActionMap[keys_with_mods] = event;
		else if (type == 3)
			scrollToActionMap[scroll] = event;
		else
			assert(false);
	else {
		Log::warn("Can't parse \"{}\" for \"{}\" action, use default value", s, parameter_name);

		type = stringToMapkey(default_value, &keys_with_mods, &scroll);
		assert(type != 0); // Because default value must be always valid
		if (type == 1)
			keyboardKeyToActionMap[keys_with_mods] = event;
		else if (type == 2)
			mouseKeyToActionMap[keys_with_mods] = event;
		else if (type == 3)
			scrollToActionMap[scroll] = event;
		else
			assert(false);
	}
}

void InputEventAdapter::parseComplexToken(string_view s, int& key, int& mods, int& key_type, bool& fully_parsed) {
	if (fully_parsed == false)
		return;

	bool found = false;
	for(const std::pair<int, string>& pair : KEYCODE_2_STRING)
		if (pair.second == s) {
			key = pair.first;
			found = true;
			key_type = 1;
			break;
	}

	if (!found) {
		for(const std::pair<int, string>& pair : MOUSEKEY_2_STRING)
			if (pair.second == s) {
				key = pair.first;
				found = true;
				key_type = 2;
				break;
			}
	}

	if (!found) {
		for(const std::pair<int, string>& pair : MODS_2_STRING)
			if (pair.second == s) {
				mods |= pair.first;
				found = true;
				break;
		}
	}

	if (!found) {
		fully_parsed = false;
		return;
	}
}

}
