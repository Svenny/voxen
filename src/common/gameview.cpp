#include <voxen/common/gameview.hpp>

#include <voxen/common/config.hpp>
#include <voxen/common/player.hpp>
#include <voxen/common/world_state.hpp>
#include <voxen/server/world.hpp>
#include <voxen/svc/message_queue.hpp>
#include <voxen/util/log.hpp>

#include <extras/math.hpp>

namespace voxen
{

GameView::GameView(os::GlfwWindow& window)
	: m_previous_tick_id(UINT64_MAX)
	, m_window(window)
	, m_is_pause(true)
	, m_is_chunk_loading_point_locked(false)
	, m_is_used_orientation_cursor(false)
{
	std::tie(m_width, m_height) = window.windowSize();
	std::pair<double, double> pos = window.cursorPos();
	m_prev_xpos = pos.first;
	m_prev_ypos = pos.second;

	m_fov_y = glm::radians(70.0);
	double tan_half_fovy = std::tan(m_fov_y / 2.0);
	double aspect = (double) m_width / (double) m_height;
	m_fov_x = 2.0 * std::atan(aspect * tan_half_fovy);

	Config* main_config = Config::mainConfig();
	m_mouse_sensitivity = main_config->getDouble("controller", "mouse_sensitivity");
	m_forward_speed = main_config->getDouble("controller", "forward_speed");
	m_strafe_speed = main_config->getDouble("controller", "strafe_speed");
	m_roll_speed = main_config->getDouble("controller", "roll_speed");

	resetKeyState();
}

void voxen::GameView::init(const voxen::Player& player) noexcept
{
	m_local_player = player;

	m_view_to_clip = extras::perspective(m_fov_x, m_fov_y, m_z_near, m_z_far);
	m_tr_world_to_view = extras::lookAt(glm::dvec3(0), player.lookVector(), player.upVector());
	m_tr_world_to_clip = m_view_to_clip * m_tr_world_to_view;

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
		}
		return true;

	default:
		return false;
	}
}

static glm::dquat quatFromEulerAngles(double pitch, double yaw, double roll) noexcept
{
	glm::dvec3 eulerAngle(pitch * 0.5, yaw * 0.5, roll * 0.5);
	glm::dvec3 c = glm::cos(eulerAngle);
	glm::dvec3 s = glm::sin(eulerAngle);

	glm::dquat res;
	res.w = c.x * c.y * c.z + s.x * s.y * s.z;
	res.x = s.x * c.y * c.z - c.x * s.y * s.z;
	res.y = c.x * s.y * c.z + s.x * c.y * s.z;
	res.z = c.x * c.y * s.z - s.x * s.y * c.z;
	return res;
}

bool voxen::GameView::handleCursor(double xpos, double ypos) noexcept
{
	m_newest_xpos = xpos;
	m_newest_ypos = ypos;
	return true;
}

void GameView::resetKeyState() noexcept
{
	for (int i = 0; i < Direction::Count; i++) {
		m_state[i] = false;
	}
}

void GameView::update(const Player& player, uint64_t tick_id, double dt, svc::MessageQueue& mq) noexcept
{
	if (m_is_pause) {
		if (!m_is_used_orientation_cursor) {
			m_window.useRegularCursor();
			m_is_used_orientation_cursor = true;
		}

		std::pair<double, double> pos = m_window.cursorPos();
		m_prev_xpos = pos.first;
		m_prev_ypos = pos.second;
	} else {
		if (m_is_used_orientation_cursor) {
			m_window.useGrabbedCursor();
			m_is_used_orientation_cursor = false;
		}

		glm::dvec3 move_forward_direction { 0.0 };
		glm::dvec3 move_strafe_direction { 0.0 };

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

		dx = (m_prev_xpos - m_newest_xpos) * m_mouse_sensitivity;
		m_prev_xpos = m_newest_xpos;
		double tan_half_fovx = std::tan(m_fov_x * 0.5);
		double yawAngle = atan(2 * dx * tan_half_fovx / m_width);

		dy = (m_prev_ypos - m_newest_ypos) * m_mouse_sensitivity;
		m_prev_ypos = m_newest_ypos;
		double tan_half_fovy = std::tan(m_fov_y * 0.5);
		double pitchAngle = atan(2 * dy * tan_half_fovy / m_height);

		double rollAngle = 0;
		if (m_state[Direction::RollLeft]) {
			rollAngle -= m_roll_speed * dt;
		}
		if (m_state[Direction::RollRight]) {
			rollAngle += m_roll_speed * dt;
		}

		auto rotQuat = quatFromEulerAngles(pitchAngle, yawAngle, rollAngle);

		glm::dvec3 new_position = m_local_player.position();
		new_position += move_forward_direction * m_forward_speed;
		new_position += move_strafe_direction * m_strafe_speed;

		glm::dquat new_orientation = glm::normalize(rotQuat * m_local_player.orientation());
		m_local_player.updateState(new_position, new_orientation);

		m_view_to_clip = extras::perspective(m_fov_x, m_fov_y, m_z_near, m_z_far);
		m_tr_world_to_view = extras::lookAt(glm::dvec3(0), m_local_player.lookVector(), m_local_player.upVector());
		m_tr_world_to_clip = m_view_to_clip * m_tr_world_to_view;

		m_previous_tick_id = tick_id;
	}

	PlayerStateMessage message;
	message.player_position = m_local_player.position();
	message.player_orientation = m_local_player.orientation();
	message.lock_chunk_loading_position = m_is_chunk_loading_point_locked;

	mq.send<PlayerStateMessage>(server::World::SERVICE_UID, message);
}

} // namespace voxen
