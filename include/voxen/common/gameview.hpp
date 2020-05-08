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
	void update (const Player& player, DebugQueueRtW& queue, int64_t tick_id) noexcept;

	bool handleKey(int key, int scancode, int action, int mods) noexcept;
	bool handleCursor(double xpos, double ypos) noexcept;
	bool handleMouseKey(int button, int action, int mods) noexcept;
	bool handleMouseScroll(double xoffset, double yoffset) noexcept;

	double fovX() const noexcept { return m_fov_x; }
	double fovY() const noexcept { return m_fov_y; }

	glm::mat4 projectionMatrix() const noexcept { return m_proj_matrix; }
	glm::mat4 viewMatrix() const noexcept { return m_view_matrix; }
	glm::mat4 cameraMatrix() const noexcept { return m_cam_matrix; }

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
	// Player orientation
	glm::dvec3 m_player_dir;
	glm::dvec3 m_player_up;
	glm::dvec3 m_player_right;
	// Gamer view data and parameters
	double m_fov_x = 1.5, m_fov_y = 1.5 * 9.0 / 16.0;
	double m_z_near = 0.1, m_z_far = 1'000'000.0;

	glm::dquat m_orientation;
	glm::mat4 m_proj_matrix;
	glm::mat4 m_view_matrix;
	glm::mat4 m_cam_matrix;

	// Previous tick id
	std::int64_t m_previous_tick_id;

	bool m_keyPressed[Key::KeyCount];

private:
	void resetKeyState() noexcept;
};

}
