#include <voxen/client/input_event_adapter.hpp>

#include <voxen/client/player_action_events.hpp>
#include <voxen/log.hpp>

#include <cassert>
#include <ranges>

using std::make_pair;
using std::string;
using std::string_view;
using std::tuple;

using voxen::os::Key;
using voxen::os::KeyModBit;
using voxen::os::MouseButton;

namespace voxen::client
{

// clang-format off: turns this array into a mess
const std::array<std::pair<Key, string>, 94> InputEventAdapter::KEYCODE_2_STRING = {
	make_pair(Key::Space, "SPACE"),
	make_pair(Key::Apostrophe, "APOSTROPHE"), /* ' */
	make_pair(Key::Comma, "COMMA"), /* , */
	make_pair(Key::Minus, "MINUS"), /* - */
	make_pair(Key::Period, "PERIOD"), /* . */
	make_pair(Key::Slash, "SLASH"), /* / */
	make_pair(Key::Digit0, "0"),
	make_pair(Key::Digit1, "1"),
	make_pair(Key::Digit2, "2"),
	make_pair(Key::Digit3, "3"),
	make_pair(Key::Digit4, "4"),
	make_pair(Key::Digit5, "5"),
	make_pair(Key::Digit6, "6"),
	make_pair(Key::Digit7, "7"),
	make_pair(Key::Digit8, "8"),
	make_pair(Key::Digit9, "9"),
	make_pair(Key::Semicolon, "SEMICOLON"), /* ; */
	make_pair(Key::Equal, "EQUAL"), /* = */
	make_pair(Key::A, "A"),
	make_pair(Key::B, "B"),
	make_pair(Key::C, "C"),
	make_pair(Key::D, "D"),
	make_pair(Key::E, "E"),
	make_pair(Key::F, "F"),
	make_pair(Key::G, "G"),
	make_pair(Key::H, "H"),
	make_pair(Key::I, "I"),
	make_pair(Key::J, "J"),
	make_pair(Key::K, "K"),
	make_pair(Key::L, "L"),
	make_pair(Key::M, "M"),
	make_pair(Key::N, "N"),
	make_pair(Key::O, "O"),
	make_pair(Key::P, "P"),
	make_pair(Key::Q, "Q"),
	make_pair(Key::R, "R"),
	make_pair(Key::S, "S"),
	make_pair(Key::T, "T"),
	make_pair(Key::U, "U"),
	make_pair(Key::V, "V"),
	make_pair(Key::W, "W"),
	make_pair(Key::X, "X"),
	make_pair(Key::Y, "Y"),
	make_pair(Key::Z, "Z"),
	make_pair(Key::LeftBracket, "["), /* [ */
	make_pair(Key::Backslash, "\\"), /* \ */
	make_pair(Key::RightBracket, "]"), /* ] */
	make_pair(Key::Grave, "`"), /* ` */
	make_pair(Key::Escape, "ESCAPE"),
	make_pair(Key::Enter, "ENTER"),
	make_pair(Key::Tab, "TAB"),
	make_pair(Key::Backspace, "BACKSPACE"),
	make_pair(Key::Insert, "INSERT"),
	make_pair(Key::Delete, "DELETE"),
	make_pair(Key::Right, "RIGHT"),
	make_pair(Key::Left, "LEFT"),
	make_pair(Key::Down, "DOWN"),
	make_pair(Key::Up, "UP"),
	make_pair(Key::PageUp, "PAGE_UP"),
	make_pair(Key::PageDown, "PAGE_DOWN"),
	make_pair(Key::Home, "HOME"),
	make_pair(Key::End, "END"),
	make_pair(Key::PrintScreen, "PRINTSCREEN"),
	make_pair(Key::Pause, "PAUSE"),
	make_pair(Key::F1, "F1"),
	make_pair(Key::F2, "F2"),
	make_pair(Key::F3, "F3"),
	make_pair(Key::F4, "F4"),
	make_pair(Key::F5, "F5"),
	make_pair(Key::F6, "F6"),
	make_pair(Key::F7, "F7"),
	make_pair(Key::F8, "F8"),
	make_pair(Key::F9, "F9"),
	make_pair(Key::F10, "F10"),
	make_pair(Key::F11, "F11"),
	make_pair(Key::F12, "F12"),
	make_pair(Key::Kp0, "NUMPAD_0"),
	make_pair(Key::Kp1, "NUMPAD_1"),
	make_pair(Key::Kp2, "NUMPAD_2"),
	make_pair(Key::Kp3, "NUMPAD_3"),
	make_pair(Key::Kp4, "NUMPAD_4"),
	make_pair(Key::Kp5, "NUMPAD_5"),
	make_pair(Key::Kp6, "NUMPAD_6"),
	make_pair(Key::Kp7, "NUMPAD_7"),
	make_pair(Key::Kp8, "NUMPAD_8"),
	make_pair(Key::Kp9, "NUMPAD_9"),
	make_pair(Key::KpDecimal, "NUMPAD_DECIMAL"),
	make_pair(Key::KpDivide, "NUMPAD_DIVIDE"),
	make_pair(Key::KpMultiply, "NUMPAD_MULTIPLY"),
	make_pair(Key::KpSubtract, "NUMPAD_SUBTRACT"),
	make_pair(Key::KpAdd, "NUMPAD_ADD"),
	make_pair(Key::KpEnter, "NUMPAD_ENTER"),
	make_pair(Key::KpEqual, "NUMPAD_EQUAL"),
	make_pair(Key::Menu, "MENU")

	// NOTE(sirgienko) Disabled because modifier keys change modifier state themselves.
	// For example, you press LCONTROL
	// Press event: LCONTROL
	// Release event: LCONTROL+CONTROL
	// So, mods setted only on release, which broke logic for some actions
	/*
	make_pair(Key::CapsLock, "CAPS_LOCK"),
	make_pair(Key::ScrollLock, "SCROLL_LOCK"),
	make_pair(Key::NumLock, "NUM_LOCK"),
	make_pair(Key::LeftShift, "LSHIFT"),
	make_pair(Key::LeftControl, "LCONTROL"),
	make_pair(Key::LeftAlt, "LALT"),
	make_pair(Key::LeftSuper, "LSUPER"),
	make_pair(Key::RightShift, "RSHIFT"),
	make_pair(Key::RightControl, "RCONTROL"),
	make_pair(Key::RightAlt, "RALT"),
	make_pair(Key::RightSuper, "RSUPER"),
	*/
};
// clang-format on

const std::array<std::pair<KeyModBit, string>, 6> InputEventAdapter::MODS_2_STRING = {
	make_pair(KeyModBit::Shift, "Shift"),
	make_pair(KeyModBit::Control, "Control"),
	make_pair(KeyModBit::Alt, "Alt"),
	make_pair(KeyModBit::Super, "Super"),
	make_pair(KeyModBit::CapsLock, "CapsLock"),
	make_pair(KeyModBit::NumLock, "Numlock"),
};

const std::array<std::pair<PlayerActionEvent, string>, 13> InputEventAdapter::PLAYERACTIONS_2_STRINGS = {
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
	make_pair(PlayerActionEvent::DecreaseSpeed, "decrease speed"),
	make_pair(PlayerActionEvent::LockChunkLoadingPoint, "lock chunk loading point"),
	make_pair(PlayerActionEvent::ModifyBlock, "modify block"),
};

const std::array<std::pair<MouseButton, string>, 5> InputEventAdapter::MOUSEKEY_2_STRING = {
	make_pair(MouseButton::Left, "MOUSE_LEFT"),
	make_pair(MouseButton::Right, "MOUSE_RIGHT"),
	make_pair(MouseButton::Middle, "MOUSE_MIDDLE"),
	make_pair(MouseButton::X1, "MOUSE_4"),
	make_pair(MouseButton::X2, "MOUSE_5"),
};

std::map<tuple<int, int>, PlayerActionEvent> InputEventAdapter::keyboardKeyToActionMap;
std::map<tuple<int, int>, PlayerActionEvent> InputEventAdapter::mouseKeyToActionMap;
std::map<bool, PlayerActionEvent> InputEventAdapter::scrollToActionMap;
string InputEventAdapter::actionSettingsPath = "configs/action_settings.toml";
Config::Scheme InputEventAdapter::actionSettingConfigScheme = InputEventAdapter::actionSettingsScheme();
Config *InputEventAdapter::actionSettingsConfig = nullptr;

namespace
{

// Lock key states are not a part of bindings
int bindingMods(os::KeyMods mods) noexcept
{
	mods.unset(KeyModBit::CapsLock);
	mods.unset(KeyModBit::NumLock);
	return mods.to_underlying();
}

} // namespace

std::pair<PlayerActionEvent, bool> InputEventAdapter::keyToPlayerEvent(const os::KeyEvent &event)
{
	// We support only press/release logic for actions at this moment,
	// so key repeats are treated as presses (activations).
	int mods = bindingMods(event.mods);

	// TODO: this is even shittier than the rest of input mapping logic
	// but I don't want to add 10 whole new mappings just for this.
	if (event.key >= Key::Digit1 && event.key <= Key::Digit0 && mods == 0) {
		int base = int(PlayerActionEvent::ChooseItem0);
		int digit = (event.key == Key::Digit0) ? 0 : int(event.key) - int(Key::Digit1) + 1;
		return make_pair(PlayerActionEvent(base + digit), event.pressed);
	}

	auto search = keyboardKeyToActionMap.find(tuple<int, int>(int(event.key), mods));
	if (search != keyboardKeyToActionMap.end()) {
		return make_pair(search->second, event.pressed);
	} else {
		return make_pair(PlayerActionEvent::None, true);
	}
}

std::pair<PlayerActionEvent, bool> InputEventAdapter::mouseButtonToPlayerEvent(const os::MouseButtonEvent &event)
{
	auto search = mouseKeyToActionMap.find(tuple<int, int>(int(event.button), bindingMods(event.mods)));
	if (search != mouseKeyToActionMap.end()) {
		return make_pair(search->second, event.pressed);
	} else {
		return make_pair(PlayerActionEvent::None, true);
	}
}

std::pair<PlayerActionEvent, bool> InputEventAdapter::mouseWheelToPlayerEvent(const os::MouseWheelEvent &event)
{
	auto search = scrollToActionMap.find(event.delta.y >= 0);
	if (search != scrollToActionMap.end()) {
		return make_pair(search->second, true);
	} else {
		return make_pair(PlayerActionEvent::None, true);
	}
}

void InputEventAdapter::init()
{
	assert(actionSettingsConfig == nullptr);
	actionSettingsConfig = new Config(actionSettingsPath, actionSettingConfigScheme);

	for (const Config::SchemeEntry &entry : actionSettingConfigScheme) {
		const string &default_value = std::get<string>(entry.default_value);
		const string &value_string
			= actionSettingsConfig->optionString(entry.section, entry.parameter_name).value_or(default_value);

		string_view value(value_string);
		PlayerActionEvent event = stringToAction(entry.parameter_name);

		string_view parameter_name(entry.parameter_name);

		if (event != PlayerActionEvent::None) {
			if (value.find(KEYS_SEQUENCE_DELIMITER) == string::npos) {
				parseToken(value, event, parameter_name, default_value);
			} else {
				for (auto token : std::views::split(value, KEYS_SEQUENCE_DELIMITER)) {
					parseToken(string_view(token.begin(), token.end()), event, parameter_name, default_value);
				}
			}
		} else {
			Log::warn("Unknown action value while parsing key action file: \"{}\" key with \"{}\" value",
				entry.parameter_name, value_string);
		}
	}
}

void InputEventAdapter::release() noexcept
{
	delete actionSettingsConfig;
	actionSettingsConfig = nullptr;
}

Config::Scheme InputEventAdapter::actionSettingsScheme()
{
	static const char *player_section = "player actions";
	Config::Scheme s;

	s.push_back({
		player_section,
		string(actionToString(PlayerActionEvent::MoveForward)),
		"Fly forward",
		keyboardKeyToString(Key::W, 0),
	});
	s.push_back({
		player_section,
		string(actionToString(PlayerActionEvent::MoveRight)),
		"Fly right",
		keyboardKeyToString(Key::D, 0),
	});
	s.push_back({
		player_section,
		string(actionToString(PlayerActionEvent::MoveLeft)),
		"Fly left",
		keyboardKeyToString(Key::A, 0),
	});
	s.push_back({
		player_section,
		string(actionToString(PlayerActionEvent::MoveBackward)),
		"Fly backward",
		keyboardKeyToString(Key::S, 0),
	});
	s.push_back({
		player_section,
		string(actionToString(PlayerActionEvent::MoveDown)),
		"Fly down",
		keyboardKeyToString(Key::C, 0),
	});
	s.push_back({
		player_section,
		string(actionToString(PlayerActionEvent::MoveUp)),
		"Fly up",
		keyboardKeyToString(Key::Space, 0),
	});
	s.push_back({
		player_section,
		string(actionToString(PlayerActionEvent::RollLeft)),
		"Roll counterclockwise",
		keyboardKeyToString(Key::Q, 0),
	});
	s.push_back({
		player_section,
		string(actionToString(PlayerActionEvent::RollRight)),
		"Rool clockwise",
		keyboardKeyToString(Key::E, 0),
	});
	s.push_back({
		player_section,
		string(actionToString(PlayerActionEvent::PauseGame)),
		"Pause movement and release cursor",
		mouseButtonToString(MouseButton::Left, 0) + string(KEYS_SEQUENCE_DELIMITER) + keyboardKeyToString(Key::Escape, 0),
	});

	s.push_back({
		player_section,
		string(actionToString(PlayerActionEvent::IncreaseSpeed)),
		"Incrase movement speed",
		scrollToString(1.0),
	});
	s.push_back({
		player_section,
		string(actionToString(PlayerActionEvent::DecreaseSpeed)),
		"Decrease movement speed",
		scrollToString(-1.0),
	});
	s.push_back({
		player_section,
		string(actionToString(PlayerActionEvent::LockChunkLoadingPoint)),
		"Lock chunk-loading position",
		keyboardKeyToString(Key::F, 0),
	});
	s.push_back({
		player_section,
		string(actionToString(PlayerActionEvent::ModifyBlock)),
		"Modify the block in front of you",
		mouseButtonToString(MouseButton::Right, 0),
	});

	return s;
}

string_view InputEventAdapter::actionToString(PlayerActionEvent event)
{
	for (auto it = PLAYERACTIONS_2_STRINGS.begin(); it != PLAYERACTIONS_2_STRINGS.end(); it++) {
		if (it->first == event) {
			return string_view(it->second);
		}
	}
	return string_view();
}

PlayerActionEvent InputEventAdapter::stringToAction(string_view s)
{
	for (auto it = PLAYERACTIONS_2_STRINGS.begin(); it != PLAYERACTIONS_2_STRINGS.end(); it++) {
		if (it->second == s) {
			return it->first;
		}
	}
	return PlayerActionEvent::None;
}

string InputEventAdapter::keyboardKeyToString(Key key, int mods)
{
	string result;

	for (auto it = KEYCODE_2_STRING.begin(); it != KEYCODE_2_STRING.end(); it++) {
		if (it->first == key) {
			result = it->second;
		}
	}

	for (auto it = MODS_2_STRING.begin(); it != MODS_2_STRING.end(); it++) {
		if (mods & int(it->first)) {
			result += string(KEYS_DELIMITER) + it->second;
		}
	}

	return result;
}

std::string InputEventAdapter::scrollToString(double yoffset)
{
	if (yoffset >= 0) {
		return string(SCROLL_UP_STRING);
	} else {
		return string(SCROLL_DOWN_STRING);
	}
}

string InputEventAdapter::mouseButtonToString(MouseButton button, int mods)
{
	string result;

	for (auto it = MOUSEKEY_2_STRING.begin(); it != MOUSEKEY_2_STRING.end(); it++) {
		if (it->first == button) {
			result = it->second;
		}
	}

	for (auto it = MODS_2_STRING.begin(); it != MODS_2_STRING.end(); it++) {
		if (mods & int(it->first)) {
			result += string(KEYS_DELIMITER) + it->second;
		}
	}

	return result;
}

int InputEventAdapter::stringToMapkey(string_view s, tuple<int, int> *tuple_ptr, bool *scroll_ptr)
{
	if (s.find(KEYS_DELIMITER) == string::npos) {
		// Simple form, just only key without any modifiers
		for (const auto &pair : KEYCODE_2_STRING) {
			if (pair.second == s) {
				*tuple_ptr = tuple<int, int>(int(pair.first), 0);
				return 1;
			}
		}
		for (const auto &pair : MOUSEKEY_2_STRING) {
			if (pair.second == s) {
				*tuple_ptr = tuple<int, int>(int(pair.first), 0);
				return 2;
			}
		}
		if (SCROLL_UP_STRING == s) {
			*scroll_ptr = true;
			return 3;
		} else if (SCROLL_DOWN_STRING == s) {
			*scroll_ptr = false;
			return 3;
		} else {
			return 0;
		}
	} else {
		int key = 0;
		int mods = 0;
		int key_type = 0; // 0 - unknown, 1 - key button, 2 - mouse button
		bool fully_parsed = true;

		for (auto subtoken : std::views::split(s, KEYS_DELIMITER)) {
			parseComplexToken(string_view(subtoken.begin(), subtoken.end()), key, mods, key_type, fully_parsed);
		}

		if (fully_parsed == false) {
			return 0;
		} else {
			if (key_type == 0) {
				return 0;
			} else if (key_type == 1) {
				*tuple_ptr = tuple<int, int>(key, mods);
				return 1;
			} else if (key_type == 2) {
				*tuple_ptr = tuple<int, int>(key, mods);
				return 2;
			} else {
				assert(false);
				return 0;
			}
		}
	}
}

void InputEventAdapter::parseToken(string_view s, PlayerActionEvent event, string_view parameter_name,
	string_view default_value)
{
	tuple<int, int> keys_with_mods;
	bool scroll;

	int type = stringToMapkey(s, &keys_with_mods, &scroll);
	if (type != 0) {
		if (type == 1) {
			keyboardKeyToActionMap[keys_with_mods] = event;
		} else if (type == 2) {
			mouseKeyToActionMap[keys_with_mods] = event;
		} else if (type == 3) {
			scrollToActionMap[scroll] = event;
		} else {
			assert(false);
		}
	} else {
		Log::warn("Can't parse \"{}\" for \"{}\" action, use default value", s, parameter_name);

		type = stringToMapkey(default_value, &keys_with_mods, &scroll);
		assert(type != 0); // Because default value must be always valid
		if (type == 1) {
			keyboardKeyToActionMap[keys_with_mods] = event;
		} else if (type == 2) {
			mouseKeyToActionMap[keys_with_mods] = event;
		} else if (type == 3) {
			scrollToActionMap[scroll] = event;
		} else {
			assert(false);
		}
	}
}

void InputEventAdapter::parseComplexToken(string_view s, int &key, int &mods, int &key_type, bool &fully_parsed)
{
	if (fully_parsed == false) {
		return;
	}

	bool found = false;
	for (const auto &pair : KEYCODE_2_STRING) {
		if (pair.second == s) {
			key = int(pair.first);
			found = true;
			key_type = 1;
			break;
		}
	}

	if (!found) {
		for (const auto &pair : MOUSEKEY_2_STRING) {
			if (pair.second == s) {
				key = int(pair.first);
				found = true;
				key_type = 2;
				break;
			}
		}
	}

	if (!found) {
		for (const auto &pair : MODS_2_STRING) {
			if (pair.second == s) {
				mods |= int(pair.first);
				found = true;
				break;
			}
		}
	}

	if (!found) {
		fully_parsed = false;
		return;
	}
}

} // namespace voxen::client
