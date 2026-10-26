#include <voxen/client/gameview.hpp>

#include <voxen/common/config.hpp>
#include <voxen/land/land_messages.hpp>
#include <voxen/land/land_public_consts.hpp>
#include <voxen/land/land_service.hpp>
#include <voxen/land/land_temp_blocks.hpp>
#include <voxen/log.hpp>
#include <voxen/os/window.hpp>
#include <voxen/svc/message_sender.hpp>
#include <voxen/world/player.hpp>
#include <voxen/world/player_state_message.hpp>
#include <voxen/world/world_control_service.hpp>

#include <m/m.hpp>

#include <cmath>

namespace voxen::client
{

GameView::GameView(os::Window& window) : m_window(window)
{
	m_fov_y = m::radians(70.0);
	setWindowSize(window.framebufferSize());

	Config* main_config = Config::mainConfig();
	m_mouse_sensitivity = main_config->getDouble("controller", "mouse_sensitivity");
	m_forward_speed = main_config->getDouble("controller", "forward_speed");
	m_strafe_speed = main_config->getDouble("controller", "strafe_speed");
	m_roll_speed = main_config->getDouble("controller", "roll_speed");

	m_current_block_id = land::TempBlockMeta::BlockStone;

	resetKeyState();
}

void GameView::init(const world::Player& player) noexcept
{
	m_local_player = player;
	m_chunk_loading_position = player.position();

	updateMatrices();

	resetKeyState();
}

bool GameView::handleEvent(client::PlayerActionEvent event, bool is_activate) noexcept
{
	switch (event) {
	case client::PlayerActionEvent::MoveForward:
		m_state[Direction::Forward] = is_activate;
		return true;

	case client::PlayerActionEvent::MoveBackward:
		m_state[Direction::Backward] = is_activate;
		return true;

	case client::PlayerActionEvent::MoveRight:
		m_state[Direction::Right] = is_activate;
		return true;

	case client::PlayerActionEvent::MoveLeft:
		m_state[Direction::Left] = is_activate;
		return true;

	case client::PlayerActionEvent::MoveUp:
		m_state[Direction::Up] = is_activate;
		return true;

	case client::PlayerActionEvent::MoveDown:
		m_state[Direction::Down] = is_activate;
		return true;

	case client::PlayerActionEvent::RollRight:
		m_state[Direction::RollRight] = is_activate;
		return true;

	case client::PlayerActionEvent::RollLeft:
		m_state[Direction::RollLeft] = is_activate;
		return true;

	case client::PlayerActionEvent::PauseGame:
		if (is_activate) {
			m_is_pause = !m_is_pause;
		}
		return true;

	case client::PlayerActionEvent::IncreaseSpeed:
		if (is_activate) {
			m_strafe_speed = 1.1 * m_strafe_speed;
			m_forward_speed = 1.1 * m_forward_speed;
		}
		return true;

	case client::PlayerActionEvent::DecreaseSpeed:
		if (is_activate) {
			m_strafe_speed = 0.9 * m_strafe_speed;
			m_forward_speed = 0.9 * m_forward_speed;
		}
		return true;

	case client::PlayerActionEvent::LockChunkLoadingPoint:
		if (is_activate) {
			m_is_chunk_loading_point_locked = !m_is_chunk_loading_point_locked;
			m_chunk_loading_position = m_local_player.position();

			const m::double3& pos = m_chunk_loading_position;
			if (m_is_chunk_loading_point_locked) {
				Log::info("Chunk loading point locked at ({:.1f}, {:.1f}, {:.1f})", pos.x, pos.y, pos.z);
			} else {
				Log::info("Chunk loading point unlocked");
			}
		}
		return true;

	case client::PlayerActionEvent::ModifyBlock:
		if (is_activate) {
			m_block_modify_requested = true;
		}
		return true;

	case client::PlayerActionEvent::ChooseItem0:
	case client::PlayerActionEvent::ChooseItem1:
	case client::PlayerActionEvent::ChooseItem2:
	case client::PlayerActionEvent::ChooseItem3:
	case client::PlayerActionEvent::ChooseItem4:
	case client::PlayerActionEvent::ChooseItem5:
	case client::PlayerActionEvent::ChooseItem6:
	case client::PlayerActionEvent::ChooseItem7:
	case client::PlayerActionEvent::ChooseItem8:
	case client::PlayerActionEvent::ChooseItem9:
		if (is_activate) {
			unsigned block_code = unsigned(event) - unsigned(client::PlayerActionEvent::ChooseItem0);
			if (block_code >= land::TempBlockMeta::NUM_BLOCKS) {
				block_code = 0;
			}

			m_current_block_id = land::Chunk::BlockId(block_code);
		}
		return true;

	default:
		return false;
	}
}

void GameView::handleMouseMotion(m::double2 delta) noexcept
{
	m_mouse_delta += delta;
}

void GameView::handleResize(Resolution framebuffer_size) noexcept
{
	if (framebuffer_size.valid()) {
		setWindowSize(framebuffer_size);
		updateMatrices();
	}
}

void GameView::setWindowSize(Resolution size) noexcept
{
	m_window_size = m::double2(size.width, size.height);

	double tan_half_fovy = std::tan(m_fov_y / 2.0);
	double aspect = m_window_size.x / m_window_size.y;
	m_fov_x = 2.0 * std::atan(aspect * tan_half_fovy);
}

void GameView::updateMatrices() noexcept
{
	const double aspect = m_window_size.x / m_window_size.y;
	m_view_to_clip = m::float4x4(m::perspectiveInfinite(m_fov_y, aspect, m_z_near));
	// Rendering is camera-relative, the view transform has no translation
	m_tr_world_to_view = m::float4x4(m::homogeneous(m::viewFromCamera(m_local_player.orientation(), m::double3())));
	m_tr_world_to_clip = m::mul(m_view_to_clip, m_tr_world_to_view);
}

void GameView::resetKeyState() noexcept
{
	for (int i = 0; i < Direction::Count; i++) {
		m_state[i] = false;
	}
}

void GameView::update(const world::Player& player, world::TickId tick_id, double dt, svc::MessageSender& msend) noexcept
{
	if (m_is_pause) {
		if (!m_is_used_orientation_cursor) {
			m_window.useRegularCursor();
			m_window.startTextInput();
			m_is_used_orientation_cursor = true;
		}

		m_mouse_delta = m::double2();
	} else {
		if (m_is_used_orientation_cursor) {
			// Don't let IME eat the controls
			m_window.stopTextInput();
			m_window.useGrabbedCursor();
			m_is_used_orientation_cursor = false;
		}

		m::double3 move_forward_direction;
		m::double3 move_strafe_direction;

		if (m_previous_tick_id < tick_id) {
			// TODO: there might be a divergence between client and server.
			// We have `m_local_player` but world state has its own player.
			// Need to somehow track state change history between players and reconverge it.
			(void) player;
		}

		double dx = 0;
		if (m_state[Direction::Left]) {
			dx -= dt;
		}
		if (m_state[Direction::Right]) {
			dx += dt;
		}
		move_strafe_direction += dx * m_local_player.rightVector();

		double dy = 0;
		if (m_state[Direction::Down]) {
			dy -= dt;
		}
		if (m_state[Direction::Up]) {
			dy += dt;
		}
		move_strafe_direction += dy * m_local_player.upVector();

		double dl = 0;
		if (m_state[Direction::Backward]) {
			dl -= dt;
		}
		if (m_state[Direction::Forward]) {
			dl += dt;
		}
		move_forward_direction += dl * m_local_player.lookVector();

		dx = -m_mouse_delta.x * m_mouse_sensitivity;
		double tan_half_fovx = std::tan(m_fov_x * 0.5);
		double yawAngle = atan(2 * dx * tan_half_fovx / m_window_size.x);

		dy = -m_mouse_delta.y * m_mouse_sensitivity;
		double tan_half_fovy = std::tan(m_fov_y * 0.5);
		double pitchAngle = atan(2 * dy * tan_half_fovy / m_window_size.y);

		m_mouse_delta = m::double2();

		double rollAngle = 0;
		if (m_state[Direction::RollLeft]) {
			rollAngle -= m_roll_speed * dt;
		}
		if (m_state[Direction::RollRight]) {
			rollAngle += m_roll_speed * dt;
		}

		// Rotations are about camera axes (looking along -Z), so they are applied
		// before the current orientation. Positive angles follow the right-hand rule.
		const m::dquat pitch = m::dquat::fromAxisAngle(m::double3(1, 0, 0), pitchAngle);
		const m::dquat yaw = m::dquat::fromAxisAngle(m::double3(0, 1, 0), yawAngle);
		const m::dquat roll = m::dquat::fromAxisAngle(m::double3(0, 0, -1), rollAngle);
		const m::dquat rotation = m::mul(yaw, m::mul(pitch, roll));

		m::double3 new_position = m_local_player.position();
		new_position += move_forward_direction * m_forward_speed;
		new_position += move_strafe_direction * m_strafe_speed;

		m::dquat new_orientation = m::normalize(m::mul(m_local_player.orientation(), rotation));
		m_local_player.updateState(new_position, new_orientation);

		updateMatrices();

		m_previous_tick_id = tick_id;
	}

	if (m_block_modify_requested) {
		m_block_modify_requested = false;

		m::int3 position = modifyTargetBlockCoord();
		land::Chunk::BlockId new_block = m_current_block_id;

		msend.send<land::BlockEditMessage>(land::LandService::SERVICE_UID, position, new_block);
	}

	if (!m_is_chunk_loading_point_locked) {
		m_chunk_loading_position = m_local_player.position();
	}

	world::PlayerStateMessage message;
	message.player_position = m_local_player.position();
	message.player_orientation = m_local_player.orientation();

	msend.send<world::PlayerStateMessage>(world::ControlService::SERVICE_UID, message);
}

m::int3 GameView::modifyTargetBlockCoord() const noexcept
{
	const m::double3 target = m_local_player.position() + 5.0 * m_local_player.lookVector();
	return m::int3(m::floor(target / land::Consts::BLOCK_SIZE_METRES));
}

} // namespace voxen::client
