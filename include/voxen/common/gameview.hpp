#pragma once

#include <voxen/client/player_action_events.hpp>
#include <voxen/common/player.hpp>
#include <voxen/common/world_state.hpp>
#include <voxen/os/glfw_window.hpp>
#include <voxen/svc/svc_fwd.hpp>
#include <voxen/land/land_chunk.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace voxen
{

class GameView {
public:
	GameView(os::GlfwWindow& window);

	void init(const Player& player) noexcept;
	void update(const Player& player, WorldTickId tick_id, double dt, svc::MessageQueue& mq) noexcept;

	bool handleEvent(client::PlayerActionEvent, bool is_activate) noexcept;
	bool handleCursor(double xpos, double ypos) noexcept;

	double fovX() const noexcept { return m_fov_x; }
	double fovY() const noexcept { return m_fov_y; }

	const glm::mat4& viewToClip() const noexcept { return m_view_to_clip; }
	const glm::mat4& translatedWorldToView() const noexcept { return m_tr_world_to_view; }
	const glm::mat4& translatedWorldToClip() const noexcept { return m_tr_world_to_clip; }
	const glm::dvec3& cameraPosition() const noexcept { return m_local_player.position(); }

	// Block ID currently selected for insertion
	auto selectedBlockId() const noexcept { return m_current_block_id; }
	// Block coordinate (in block space, not world space) where the player is currently targeting
	glm::ivec3 modifyTargetBlockCoord() const noexcept;

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
	// Gamer view data and parameters
	double m_fov_x, m_fov_y;
	double m_z_near = 0.1, m_z_far = 1'000'000.0;

	// Local copy of player state - updated directly here instead of
	// requesting it from world, eliminates lag from messaging latency.
	Player m_local_player;

	glm::mat4 m_view_to_clip;
	glm::mat4 m_tr_world_to_view;
	glm::mat4 m_tr_world_to_clip;

	// Previous tick id
	WorldTickId m_previous_tick_id;
	os::GlfwWindow& m_window;

	// TODO This is temporary solution, we should replace then add pause widget to Gui stack
	bool m_is_pause = true;
	bool m_is_chunk_loading_point_locked = false;
	bool m_is_used_orientation_cursor = false;
	bool m_block_modify_requested = false;
	land::Chunk::BlockId m_current_block_id = 0;

	bool m_state[Direction::Count];

private:
	void resetKeyState() noexcept;
};

} // namespace voxen
