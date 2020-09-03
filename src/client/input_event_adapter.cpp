#include <voxen/client/input_event_adapter.hpp>
#include <voxen/common/filemanager.hpp>

#include <cassert>
#include <voxen/util/log.hpp>

#include <GLFW/glfw3.h>
#include <extras/string_utils.hpp>

using std::uint32_t;
using std::make_pair;
namespace fs = std::filesystem;

namespace voxen::client {

const std::array<std::pair<int, std::string>, 94> InputEventAdapter::KEYCODE_2_STRING = {
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

const std::array<std::pair<int, std::string>, 6> InputEventAdapter::MODS_2_STRING = {
	make_pair(GLFW_MOD_SHIFT, "Shift"),
	make_pair(GLFW_MOD_CONTROL, "Control"),
	make_pair(GLFW_MOD_ALT, "Alt"),
	make_pair(GLFW_MOD_SUPER, "Super"),
	make_pair(GLFW_MOD_CAPS_LOCK, "CapsLock"),
	make_pair(GLFW_MOD_NUM_LOCK, "Numlock")
};

const std::array<std::pair<PlayerActionEvents, std::string>, 11> InputEventAdapter::PLAYERACTIONS_2_STRINGS= {
    make_pair(PlayerActionEvents::MoveForward, "move forward"),
    make_pair(PlayerActionEvents::MoveBackward, "move backward"),
    make_pair(PlayerActionEvents::MoveUp, "move up"),
    make_pair(PlayerActionEvents::MoveDown, "stop move down"),
    make_pair(PlayerActionEvents::MoveLeft, "move left"),
    make_pair(PlayerActionEvents::MoveRight, "move right"),
    make_pair(PlayerActionEvents::RollLeft, "roll left"),
    make_pair(PlayerActionEvents::RollRight, "roll right"),
    make_pair(PlayerActionEvents::PauseGame, "pause game"),
    make_pair(PlayerActionEvents::IncreaseSpeed, "increase speed"),
    make_pair(PlayerActionEvents::DecreaseSpeed, "decrease speed")
};

const std::array<std::pair<int, std::string>, 8> InputEventAdapter::MOUSEKEY_2_STRING = {
	make_pair(GLFW_MOUSE_BUTTON_LEFT, "MOUSE_LEFT"),
	make_pair(GLFW_MOUSE_BUTTON_RIGHT, "MOUSE_RIGHT"),
	make_pair(GLFW_MOUSE_BUTTON_MIDDLE, "MOUSE_MIDDLE"),
	make_pair(GLFW_MOUSE_BUTTON_4, "MOUSE_4"),
	make_pair(GLFW_MOUSE_BUTTON_5, "MOUSE_5"),
	make_pair(GLFW_MOUSE_BUTTON_6, "MOUSE_6"),
	make_pair(GLFW_MOUSE_BUTTON_7, "MOUSE_7"),
	make_pair(GLFW_MOUSE_BUTTON_8, "MOUSE_8")
};

std::map<uint32_t, PlayerActionEvents> InputEventAdapter::keyToActionMap;
std::string InputEventAdapter::actionSettingsPath = "configs/action_settings.ini";
Config::Scheme InputEventAdapter::actionSettingConfigScheme = InputEventAdapter::actionSettingsScheme();
Config* InputEventAdapter::actionSettingsConfig = nullptr;

std::pair<PlayerActionEvents, bool> InputEventAdapter::glfwKeyboardToPlayerEvent(int key, int scancode, int action, int mods) {
	(void)scancode;

	uint32_t key_hash = hashGlfwKeyInput(key, mods);
	auto search = keyToActionMap.find(key_hash);
	if (search != keyToActionMap.end())
		return make_pair(search->second, action != GLFW_RELEASE);
	else
		return make_pair(PlayerActionEvents::None, true);
}

std::pair<PlayerActionEvents, bool> InputEventAdapter::glfwMouseKeyToPlayerEvent(int button, int action, int mods) {
	// `action` have three state: press, release, repeat. We support for actions only press/release logic on this moment, so
	// so pass only values with two state: press/repreate - true and release - false
	// More info: https://www.glfw.org/docs/3.3.2/group__input.html#ga2485743d0b59df3791c45951c4195265

	uint32_t key_hash = hashGlfwMouseKeyInput(button, mods);
	auto search = keyToActionMap.find(key_hash);
	if (search != keyToActionMap.end())
		return make_pair(search->second, action != GLFW_RELEASE);
	else
		return make_pair(PlayerActionEvents::None, true);
}

std::pair<PlayerActionEvents, bool> InputEventAdapter::glfwMouseScrollToPlayerEvent(double xoffset, double yoffset) {
	(void)xoffset;

	uint32_t key_hash = hashGlfwMouseScrollInput(yoffset);
	auto search = keyToActionMap.find(key_hash);
    if (search != keyToActionMap.end())
		return make_pair(search->second, true);
	else
		return make_pair(PlayerActionEvents::None, true);
}

void InputEventAdapter::init() {
	assert(actionSettingsConfig == nullptr);
	actionSettingsConfig = new Config(FileManager::userDataPath() / actionSettingsPath, actionSettingConfigScheme);

	std::function<void(std::string_view, voxen::client::PlayerActionEvents, std::string_view, std::string_view)> functor = InputEventAdapter::parseToken;
	for (const Config::SchemeEntry& entry : actionSettingConfigScheme) {
		const std::string& value_string = actionSettingsConfig->optionString(entry.section, entry.parameter_name);

		std::string_view value(value_string);
		PlayerActionEvents event = stringToAction(entry.parameter_name);

		const std::string& default_value = std::get<std::string>(entry.default_value);
		std::string_view parameter_name(entry.parameter_name);

		if (event != PlayerActionEvents::None) {
			if (value.find(KEYS_SEQUENCE_DELIMITER) == std::string::npos)
				parseToken(value, event, parameter_name, default_value);
			else
				string_split_apply(value, KEYS_SEQUENCE_DELIMITER, [event, &default_value, parameter_name](std::string_view string){
					parseToken(string, event, parameter_name, default_value);
				});
		}
		else
			Log::warn("Unknown action value while parsing key action file: \"{}\" key with \"{}\" value", entry.parameter_name, value_string);
	}
}

void InputEventAdapter::release() {
	delete actionSettingsConfig;
	actionSettingsConfig = nullptr;
}

Config::Scheme InputEventAdapter::actionSettingsScheme() {
	static const char* player_section = "player actions";
	Config::Scheme s;

	s.push_back({player_section, std::string(actionToString(PlayerActionEvents::MoveForward)), "Fly forward", hashToString(hashGlfwKeyInput(GLFW_KEY_W, 0))});
	s.push_back({player_section, std::string(actionToString(PlayerActionEvents::MoveRight)), "Fly right", hashToString(hashGlfwKeyInput(GLFW_KEY_D, 0))});
	s.push_back({player_section, std::string(actionToString(PlayerActionEvents::MoveLeft)), "Fly left", hashToString(hashGlfwKeyInput(GLFW_KEY_A, 0))});
	s.push_back({player_section, std::string(actionToString(PlayerActionEvents::MoveBackward)), "Fly backward", hashToString(hashGlfwKeyInput(GLFW_KEY_S, 0))});
	s.push_back({player_section, std::string(actionToString(PlayerActionEvents::MoveDown)), "Fly down", hashToString(hashGlfwKeyInput(GLFW_KEY_C, 0))});
	s.push_back({player_section, std::string(actionToString(PlayerActionEvents::MoveUp)), "Fly up", hashToString(hashGlfwKeyInput(GLFW_KEY_SPACE, 0))});
	s.push_back({player_section, std::string(actionToString(PlayerActionEvents::RollLeft)), "Roll counterclockwise", hashToString(hashGlfwKeyInput(GLFW_KEY_Q, 0))});
	s.push_back({player_section, std::string(actionToString(PlayerActionEvents::RollRight)), "Rool clockwise", hashToString(hashGlfwKeyInput(GLFW_KEY_E, 0))});
	s.push_back({
		player_section, std::string(actionToString(PlayerActionEvents::PauseGame)), "Pause movement and release cursor",
			hashToString(hashGlfwMouseKeyInput(GLFW_MOUSE_BUTTON_LEFT, 0)) + std::string(KEYS_SEQUENCE_DELIMITER) + hashToString(hashGlfwKeyInput(GLFW_KEY_ESCAPE, 0))
	});

	s.push_back({player_section, std::string(actionToString(PlayerActionEvents::IncreaseSpeed)), "Incrase movement speed", hashToString(hashGlfwMouseScrollInput(1.0))});
	s.push_back({player_section, std::string(actionToString(PlayerActionEvents::DecreaseSpeed)), "Decrease movement speed", hashToString(hashGlfwMouseScrollInput(-1.0))});

	return s;
}

uint32_t InputEventAdapter::hashGlfwKeyInput(int key, int mods) {
	uint32_t hash = 0U;

	// Get 9 bits, because this is actually a range of GLFW `key`
	// More info: https://www.glfw.org/docs/3.3.2/group__keys.html
	hash = key & 0x1FF;

	// `mods` is flags and have wide in 2*4 = 8 bit
	// More info: https://www.glfw.org/docs/3.3.2/group__mods.html
	hash |= (mods & 0xFF ) << 9;

	// And we have three type of input: from keyboard, mouse and scroll.
	// So, use first two bytes for GLFW event type
	// 00 - keyboard, 01 - mouse, 10 - scroll, 11 - invalid hash

	// Summary: 9 + 8 + 2 = 19 bits
	return hash;
}

uint32_t InputEventAdapter::hashGlfwMouseKeyInput(int button, int mods) {
	// See hashGlfwKeyInput and GLFW event type explanation
	uint32_t hash = 0x40000000; // This is 30^2

	// `button` have only 3 significant bits, becuase GLFW have only 8 mouse group__keys
	// More info: https://www.glfw.org/docs/3.3.2/group__buttons.html
	hash |= button & 0x7;

	// See explanation for `mods` in hashGlfwKeyInput
	hash |= (mods & 0xFF) << 3;

	// Summary: 3 + 8 + 2 = 13 bits
	return hash;
}

uint32_t InputEventAdapter::hashGlfwMouseScrollInput(double yoffset) {
	// See hashGlfwKeyInput and GLFW event type explanation
	uint32_t hash = 0x80000000; // This is 31^2

	// We support only simple logic with scrolling input
	// Two states: yoffset >=0 and yoffset < 0
	// So, we need only one bit

	if (yoffset >= 0)
		hash |= 0x1;

	// Summary: 1 + 2 = 3 bits
	return hash;
}

std::string InputEventAdapter::hashToString(uint32_t hash) {
	unsigned int event_type = hash >> 30;
	if (event_type == 0) { // keyboard input
		int key = hash & 0x1FF;
		int mods = (hash >> 9) & 0xFF;
		std::string hash_string(keyToString(key));
		for (auto it = MODS_2_STRING.begin(); it != MODS_2_STRING.end(); it++)
			if (mods & it->first)
				hash_string += std::string(KEYS_DELIMITER)+it->second;
		return hash_string;
	}
	else if (event_type == 1) { // mouse key input
		int button = hash & 0x7;
		int mods = (hash >> 3) & 0xFF;
		std::string hash_string(mousekeyToString(button));
		for (auto it = MODS_2_STRING.begin(); it != MODS_2_STRING.end(); it++)
			if (mods & it->first)
				hash_string += std::string(KEYS_DELIMITER)+it->second;
		return hash_string;
	}
	else if (event_type == 2) { // scroll input
		if (hash & 0x1)
			return std::string(SCROLL_UP_STRING);
		else
			return std::string(SCROLL_DOWN_STRING);
	}
	else
		assert(false);
	return "test-string";
}

std::string_view InputEventAdapter::actionToString(PlayerActionEvents event) {
	for (auto it = PLAYERACTIONS_2_STRINGS.begin(); it != PLAYERACTIONS_2_STRINGS.end(); it++) {
		if (it->first == event)
			return std::string_view(it->second);
	}
	return std::string_view();
}

PlayerActionEvents InputEventAdapter::stringToAction(std::string_view s) {
for (auto it = PLAYERACTIONS_2_STRINGS.begin(); it != PLAYERACTIONS_2_STRINGS.end(); it++) {
		if (it->second == s)
			return it->first;
	}
	return PlayerActionEvents::None;
}

std::string_view InputEventAdapter::keyToString(int key) {
	for (auto it = KEYCODE_2_STRING.begin(); it != KEYCODE_2_STRING.end(); it++) {
		if (it->first == key)
			return std::string_view(it->second);
	}
	return std::string_view();
}

std::string_view InputEventAdapter::mousekeyToString(int key) {
	for (auto it = MOUSEKEY_2_STRING.begin(); it != MOUSEKEY_2_STRING.end(); it++) {
		if (it->first == key)
			return std::string_view(it->second);
	}
	return std::string_view();
}

std::uint32_t InputEventAdapter::stringToHash(std::string_view s) {
	size_t pos = 0;
	(void)pos;
	if (s.find(KEYS_DELIMITER) == std::string::npos) {
		// Simple form, just only key without any modifiers
		for(const std::pair<int, std::string>& pair : KEYCODE_2_STRING)
			if (pair.second == s)
				return hashGlfwKeyInput(pair.first, 0);
		for(const std::pair<int, std::string>& pair : MOUSEKEY_2_STRING)
			if (pair.second == s)
				return hashGlfwMouseKeyInput(pair.first, 0);
		if (SCROLL_UP_STRING == s)
			return hashGlfwMouseScrollInput(1.0);
		else if (SCROLL_DOWN_STRING == s)
			return hashGlfwMouseScrollInput(-1.0);
		else {
			return INVALID_HASH;
		}
	}
	else {
		int key = 0;
		int mods = 0;
		int key_type = 0; //0 - unknown, 1 - key button, 2 - mouse button
		bool fully_parsed = true;

		string_split_apply(s, KEYS_DELIMITER, [&key, &mods, &key_type, &fully_parsed](std::string_view string){
			parseComplexToken(string, key, mods, key_type, fully_parsed);
		});

		if (fully_parsed == false)
			return INVALID_HASH;
		else {
			if (key_type == 0)
				return INVALID_HASH;
			else if (key_type == 1)
				return hashGlfwKeyInput(key, mods);
			else if (key_type == 2)
				return hashGlfwMouseKeyInput(key, mods);
			else {
				assert(false);
				return INVALID_HASH;
			}
		}
	}
}

void InputEventAdapter::parseToken(std::string_view string, PlayerActionEvents event, std::string_view parameter_name, std::string_view default_value) {
	uint32_t hash = stringToHash(string);
	if (hash != INVALID_HASH)
		keyToActionMap[hash] = event;
	else {
		Log::warn("Can't parse \"{}\" for \"{}\" action, use default value", string, parameter_name);
		keyToActionMap[stringToHash(default_value)] = event;
	}
}

void InputEventAdapter::parseComplexToken(std::string_view string, int& key, int& mods, int& key_type, bool& fully_parsed) {
	if (fully_parsed == false)
		return;

	bool found = false;
	for(const std::pair<int, std::string>& pair : KEYCODE_2_STRING)
		if (pair.second == string) {
			key = pair.first;
			found = true;
			key_type = 1;
			break;
	}

	if (!found) {
		for(const std::pair<int, std::string>& pair : MOUSEKEY_2_STRING)
			if (pair.second == string) {
				key = pair.first;
				found = true;
				key_type = 2;
				break;
			}
	}

	if (!found) {
		for(const std::pair<int, std::string>& pair : MODS_2_STRING)
			if (pair.second == string) {
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
