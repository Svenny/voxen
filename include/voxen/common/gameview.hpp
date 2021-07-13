#pragma once

#include <voxen/client/window.hpp>
#include <voxen/client/player_action_events.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <chrono>

namespace voxen
{

class Player;
struct DebugQueueRtW;

class GameView {
public:
	GameView(client::Window& window);

	void init(const Player& player) noexcept;
	void update (const Player& player, DebugQueueRtW& queue, uint64_t tick_id) noexcept;

	bool handleEvent(client::PlayerActionEvent, bool is_activate) noexcept;
	bool handleCursor(double xpos, double ypos) noexcept;

	double fovX() const noexcept { return m_fov_x; }
	double fovY() const noexcept { return m_fov_y; }

	const glm::mat4 &viewToClip() const noexcept { return m_view_to_clip; }
	const glm::mat4 &translatedWorldToView() const noexcept { return m_tr_world_to_view; }
	const glm::mat4 &translatedWorldToClip() const noexcept { return m_tr_world_to_clip; }
	const glm::dvec3 &cameraPosition() const noexcept { return m_cam_position; }

private:
	enum Direction : int {
		Forward = 0,
		Backward,
		Left,
		Right,
		Up,
		Down,
		RollLeft,
		RollRight,

		Count
	};

	double m_width, m_height;

	double m_mouse_sensitivity;
	double m_forward_speed;
	double m_strafe_speed;
	double m_roll_speed;

	// Mouse position at the time of the latest call to `handleCursor`
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
	double m_fov_x, m_fov_y;
	double m_z_near = 0.1, m_z_far = 1'000'000.0;

	glm::dvec3 m_cam_position;
	glm::dquat m_cam_orientation;
	glm::mat4 m_view_to_clip;
	glm::mat4 m_tr_world_to_view;
	glm::mat4 m_tr_world_to_clip;

	// Previous tick id
	std::uint64_t m_previous_tick_id;
	client::Window& m_window;

	// TODO This is temporary solution, we should replace then add pause widget to Gui stack
	bool m_is_pause;
	bool m_is_used_orientation_cursor;

	bool m_state[Direction::Count];

private:
	void resetKeyState() noexcept;
};

}
