#pragma once

#include <voxen/util/resolution.hpp>

#include <extras/enum_utils.hpp>

#include <m/vector.hpp>

#include <cstdint>
#include <string>
#include <variant>

namespace voxen::os
{

// Physical keyboard key, identified by its position regardless of the active layout.
// Names follow the US QWERTY layout, e.g. `Key::W` is the key right of Tab on any layout.
// Values are USB HID usage IDs (Keyboard/Keypad page 0x07); keys without one are `Unknown`.
enum class Key : uint16_t {
	Unknown = 0,

	A = 4,
	B = 5,
	C = 6,
	D = 7,
	E = 8,
	F = 9,
	G = 10,
	H = 11,
	I = 12,
	J = 13,
	K = 14,
	L = 15,
	M = 16,
	N = 17,
	O = 18,
	P = 19,
	Q = 20,
	R = 21,
	S = 22,
	T = 23,
	U = 24,
	V = 25,
	W = 26,
	X = 27,
	Y = 28,
	Z = 29,

	// Top row digits, `1` to `9` then `0`
	Digit1 = 30,
	Digit2 = 31,
	Digit3 = 32,
	Digit4 = 33,
	Digit5 = 34,
	Digit6 = 35,
	Digit7 = 36,
	Digit8 = 37,
	Digit9 = 38,
	Digit0 = 39,

	Enter = 40,
	Escape = 41,
	Backspace = 42,
	Tab = 43,
	Space = 44,
	Minus = 45,
	Equal = 46,
	LeftBracket = 47,
	RightBracket = 48,
	Backslash = 49,
	// ISO keyboards only, next to Enter
	NonUsHash = 50,
	Semicolon = 51,
	Apostrophe = 52,
	Grave = 53,
	Comma = 54,
	Period = 55,
	Slash = 56,
	CapsLock = 57,

	F1 = 58,
	F2 = 59,
	F3 = 60,
	F4 = 61,
	F5 = 62,
	F6 = 63,
	F7 = 64,
	F8 = 65,
	F9 = 66,
	F10 = 67,
	F11 = 68,
	F12 = 69,

	PrintScreen = 70,
	ScrollLock = 71,
	Pause = 72,
	Insert = 73,
	Home = 74,
	PageUp = 75,
	Delete = 76,
	End = 77,
	PageDown = 78,
	Right = 79,
	Left = 80,
	Down = 81,
	Up = 82,

	NumLock = 83,
	KpDivide = 84,
	KpMultiply = 85,
	KpSubtract = 86,
	KpAdd = 87,
	KpEnter = 88,
	Kp1 = 89,
	Kp2 = 90,
	Kp3 = 91,
	Kp4 = 92,
	Kp5 = 93,
	Kp6 = 94,
	Kp7 = 95,
	Kp8 = 96,
	Kp9 = 97,
	Kp0 = 98,
	KpDecimal = 99,
	// ISO keyboards only, between left Shift and Z
	NonUsBackslash = 100,
	// Context menu key
	Menu = 101,
	KpEqual = 103,

	F13 = 104,
	F14 = 105,
	F15 = 106,
	F16 = 107,
	F17 = 108,
	F18 = 109,
	F19 = 110,
	F20 = 111,
	F21 = 112,
	F22 = 113,
	F23 = 114,
	F24 = 115,

	LeftControl = 224,
	LeftShift = 225,
	LeftAlt = 226,
	LeftSuper = 227,
	RightControl = 228,
	RightShift = 229,
	RightAlt = 230,
	RightSuper = 231,

	// Not a key, upper bound of the values
	EnumSize = 232,
};

// Modifier key state, left and right keys are not distinguished
enum class KeyModBit : uint16_t {
	None = 0,
	Shift = 1u << 0,
	Control = 1u << 1,
	Alt = 1u << 2,
	Super = 1u << 3,
	// Lock key states (not whether they are pressed)
	CapsLock = 1u << 4,
	NumLock = 1u << 5,
};

// Bitmask of `KeyModBit`
using KeyMods = extras::enum_flags<KeyModBit>;

enum class MouseButton : uint8_t {
	Left,
	Right,
	Middle,
	// "Back" and "forward" side buttons
	X1,
	X2,

	EnumSize,
};

// All positions are in framebuffer pixels, relative to the top-left corner of the window's client area

// A physical key was pressed or released
struct KeyEvent {
	Key key = Key::Unknown;
	// Modifier state at the time of the event
	KeyMods mods;
	bool pressed = false;
	// Auto-repeated press of a held key
	bool repeat = false;
};

// Text was entered: typed with the current keyboard layout or committed by an IME.
// Separate from `KeyEvent`s, one key press can produce zero, one or several characters.
// Delivered only while text input is enabled, see `Window::startTextInput()`.
struct TextInputEvent {
	// UTF-8
	std::string text;
};

// IME composition (preedit) text changed. Applications that show the composition
// themselves draw `text` at the input position with `[start, start + length)`
// highlighted; empty `text` ends the composition. The text being composed
// is not committed, it arrives with `TextInputEvent` when the user confirms it.
struct TextEditingEvent {
	// UTF-8
	std::string text;
	// Selected (being converted) part of the text, in UTF-8 code points
	int32_t start = 0;
	int32_t length = 0;
};

struct MouseButtonEvent {
	MouseButton button = MouseButton::Left;
	KeyMods mods;
	bool pressed = false;
	// Number of consecutive clicks (1 = single, 2 = double...) for presses, 0 for releases
	int32_t clicks = 0;
	m::double2 position;
};

struct MouseMotionEvent {
	// Cursor position, not meaningful while the cursor is grabbed (see `Window::useGrabbedCursor()`)
	m::double2 position;
	// Relative motion since the previous event, also delivered while the cursor is grabbed
	m::double2 delta;
};

struct MouseWheelEvent {
	// Positive Y is scrolling up (away from the user), positive X is scrolling right
	m::double2 delta;
};

// Framebuffer size of the window has changed
struct WindowResizeEvent {
	Resolution framebuffer_size;
};

// User asked to close the window, `Window::shouldClose()` returns true from now on
struct WindowCloseEvent {};

using WindowEvent = std::variant<KeyEvent, TextInputEvent, TextEditingEvent, MouseButtonEvent, MouseMotionEvent,
	MouseWheelEvent, WindowResizeEvent, WindowCloseEvent>;

} // namespace voxen::os
