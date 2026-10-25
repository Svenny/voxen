#pragma once

#include <voxen/client/player_action_events.hpp>
#include <voxen/common/player.hpp>
#include <voxen/land/land_chunk.hpp>
#include <voxen/os/os_fwd.hpp>
#include <voxen/svc/svc_fwd.hpp>
#include <voxen/util/resolution.hpp>
#include <voxen/world/world_tick_id.hpp>

#include <m/m.hpp>

namespace voxen
{

class GameView {
public:
	GameView(os::Window& window);

	void init(const Player& player) noexcept;
	void update(const Player& player, world::TickId tick_id, double dt, svc::MessageSender& msend) noexcept;

	bool handleEvent(client::PlayerActionEvent, bool is_activate) noexcept;
	// Relative mouse motion in framebuffer pixels
	void handleMouseMotion(m::double2 delta) noexcept;
	void handleResize(Resolution framebuffer_size) noexcept;

	double fovX() const noexcept { return m_fov_x; }
	double fovY() const noexcept { return m_fov_y; }

	const m::float4x4& viewToClip() const noexcept { return m_view_to_clip; }
	const m::float4x4& translatedWorldToView() const noexcept { return m_tr_world_to_view; }
	const m::float4x4& translatedWorldToClip() const noexcept { return m_tr_world_to_clip; }
	const m::double3& cameraPosition() const noexcept { return m_local_player.position(); }
	// Center of the land loading area: follows the camera unless locked by `LockChunkLoadingPoint`
	const m::double3& chunkLoadingPosition() const noexcept { return m_chunk_loading_position; }

	// Block ID currently selected for insertion
	auto selectedBlockId() const noexcept { return m_current_block_id; }
	// Block coordinate (in block space, not world space) where the player is currently targeting
	m::int3 modifyTargetBlockCoord() const noexcept;

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

	m::double2 m_window_size;

	double m_mouse_sensitivity;
	double m_forward_speed;
	double m_strafe_speed;
	double m_roll_speed;

	// Mouse motion accumulated since the latest call to `update`
	m::double2 m_mouse_delta;
	// Gamer view data and parameters
	double m_fov_x, m_fov_y;
	double m_z_near = 0.1;

	// Local copy of player state - updated directly here instead of
	// requesting it from world, eliminates lag from messaging latency.
	Player m_local_player;
	m::double3 m_chunk_loading_position;

	m::float4x4 m_view_to_clip;
	m::float4x4 m_tr_world_to_view;
	m::float4x4 m_tr_world_to_clip;

	// Previous tick id
	world::TickId m_previous_tick_id;
	os::Window& m_window;

	// TODO This is temporary solution, we should replace then add pause widget to Gui stack
	bool m_is_pause = true;
	bool m_is_chunk_loading_point_locked = false;
	bool m_is_used_orientation_cursor = false;
	bool m_block_modify_requested = false;
	land::Chunk::BlockId m_current_block_id = 0;

	bool m_state[Direction::Count];

private:
	void setWindowSize(Resolution size) noexcept;
	void updateMatrices() noexcept;
	void resetKeyState() noexcept;
};

} // namespace voxen
