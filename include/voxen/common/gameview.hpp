#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <chrono>

namespace voxen {


class Player;
struct DebugQueueRtW;
class GameView {
public:
	GameView();

	void init(const Player& player);
	void update (const Player& player, DebugQueueRtW& queue) noexcept;

	bool handleKey(int key, int scancode, int action, int mods) noexcept;
	bool handleCursor(double xpos, double ypos) noexcept;
	bool handleMouseKey(int button, int action, int mods) noexcept;
	bool handleMouseScroll(double xoffset, double yoffset) noexcept;

private:
	enum Key : int {
		KeyForward = 0,
		KeyBack,
		KeyLeft,
		KeyRight,
		KeyUp,
		KeyDown,
		KeyRollCW,
		KeyRollCCW,

		KeyCount
	};

	double m_width, m_height;

	double m_mouseSensitivity;
	double m_forwardSpeed;
	double m_strafeSpeed;

	// Mouse position at the time of the latest call to `handleCursor`
	bool m_was_mouse_move = false;
	double m_newest_xpos;
	double m_newest_ypos;
	// Mouse position at the time of the latest call to `update`
	double m_prev_xpos;
	double m_prev_ypos;

	bool m_keyPressed[Key::KeyCount];

private:
	void resetKeyState() noexcept;
};

}
