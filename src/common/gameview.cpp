#include <voxen/common/gameview.hpp>

#include <voxen/common/world_state.hpp>
#include <voxen/common/player.hpp>
#include <voxen/common/config.hpp>

#include <voxen/util/log.hpp>

#include <extras/math.hpp>

#include <glm/gtc/quaternion.hpp>

#include <GLFW/glfw3.h>

using namespace voxen;

GameView::GameView (client::Window& window):
	m_previous_tick_id(UINT64_MAX), m_window(window),
	m_is_pause(true), m_is_used_orientation_cursor(false) {
	m_width = window.width();
	m_height = window.height();
	std::pair<double, double> pos = window.cursorPos();
	m_prev_xpos = pos.first;
	m_prev_ypos = pos.second;

	m_fov_x = 1.5;
	m_fov_y = 1.5 * (double)window.height() / (double)window.width();

	Config* main_config = Config::mainConfig();
	m_mouse_sensitivity = main_config->optionDouble("controller", "mouse_sensitivity");
	m_forward_speed = main_config->optionDouble("controller", "forward_speed");
	m_strafe_speed = main_config->optionDouble("controller", "strafe_speed");
	m_roll_speed = main_config->optionDouble("controller", "roll_speed");

	resetKeyState();
}

void voxen::GameView::init(const voxen::Player& player) noexcept {
	m_player_dir = player.lookVector();
	m_player_up = player.upVector();
	m_player_right = player.rightVector();

	m_cam_position = player.position();
	m_cam_orientation = player.orientation();
	m_view_to_clip = extras::perspective(m_fov_x, m_fov_y, m_z_near, m_z_far);
	m_tr_world_to_view = extras::lookAt(glm::dvec3(0), m_player_dir, m_player_up);
	m_tr_world_to_clip = m_view_to_clip * m_tr_world_to_view;

	resetKeyState();
}
bool GameView::handleEvent(client::PlayerActionEvent event, bool is_activate) noexcept
{
	switch (event)
	{
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
			if (is_activate)
				m_is_pause = !m_is_pause;
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

		default:
			return false;
	}
}

static glm::dquat quatFromEulerAngles (double pitch, double yaw, double roll) noexcept {
	glm::dvec3 eulerAngle (pitch * 0.5, yaw * 0.5, roll * 0.5);
	glm::dvec3 c = glm::cos (eulerAngle);
	glm::dvec3 s = glm::sin (eulerAngle);

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

void GameView::resetKeyState() noexcept {
	for (int i = 0; i < Direction::Count; i++)
		m_state[i] = false;
}

void GameView::update (const Player& player, DebugQueueRtW& queue, uint64_t tick_id) noexcept {
	std::lock_guard<std::mutex> lock { queue.mutex };

	if (m_is_pause) {
		if (!m_is_used_orientation_cursor) {
			m_window.useRegularCursor();
			m_is_used_orientation_cursor = true;
		}

		std::pair<double, double> pos = m_window.cursorPos();
		m_prev_xpos = pos.first;
		m_prev_ypos = pos.second;

		queue.player_forward_movement_direction = glm::dvec3{0.0};
		queue.player_strafe_movement_direction = glm::dvec3{0.0};
	} else {
		if (m_is_used_orientation_cursor) {
			m_window.useOrientationCursor();
			m_is_used_orientation_cursor = false;
		}

		glm::dvec3 move_forward_direction{0.0};
		glm::dvec3 move_strafe_direction{0.0};

		if (m_previous_tick_id < tick_id) {
			m_player_dir = player.lookVector();
			m_player_up = player.upVector();
			m_player_right = player.rightVector();

			m_cam_position = player.position();
			m_cam_orientation = player.orientation();
		}

		double dx = 0;
		if (m_state[Direction::Left]) dx -= 1;
		if (m_state[Direction::Right]) dx += 1;
		move_strafe_direction += dx * m_player_right;

		double dy = 0;
		if (m_state[Direction::Down]) dy -= 1;
		if (m_state[Direction::Up]) dy += 1;
		move_strafe_direction += dy * m_player_up;

		double dl = 0;
		if (m_state[Direction::Backward]) dl -= 1;
		if (m_state[Direction::Forward]) dl += 1;
		move_forward_direction += dl * m_player_dir;

		dx = (m_prev_xpos - m_newest_xpos) * m_mouse_sensitivity;
		m_prev_xpos = m_newest_xpos;
		double tan_half_fovx = std::tan(m_fov_x * 0.5);
		double yawAngle = atan (2 * dx * tan_half_fovx / m_width);

		dy = (m_prev_ypos - m_newest_ypos) * m_mouse_sensitivity;
		m_prev_ypos = m_newest_ypos;
		double tan_half_fovy = std::tan(m_fov_y * 0.5);
		double pitchAngle = atan (2 * dy * tan_half_fovy / m_height);

		double rollAngle = 0;
		if (m_state[Direction::RollLeft]) rollAngle -= m_roll_speed;
		if (m_state[Direction::RollRight]) rollAngle += m_roll_speed;

		auto rotQuat = quatFromEulerAngles(pitchAngle, yawAngle, rollAngle);
		m_cam_orientation = glm::normalize(rotQuat * m_cam_orientation);

		glm::dmat3 rot_mat = glm::mat3_cast(m_cam_orientation);
		m_player_dir = extras::dirFromOrientation(rot_mat);
		m_player_right = extras::rightFromOrientation(rot_mat);
		m_player_up = extras::upFromOrientation(rot_mat);

		m_view_to_clip = extras::perspective(m_fov_x, m_fov_y, m_z_near, m_z_far);
		m_tr_world_to_view = extras::lookAt(glm::dvec3(0), m_player_dir, m_player_up);
		m_tr_world_to_clip = m_view_to_clip * m_tr_world_to_view;

		queue.player_forward_movement_direction = move_forward_direction;
		queue.player_strafe_movement_direction = move_strafe_direction;
		queue.player_orientation = m_cam_orientation;
		queue.forward_speed = m_forward_speed;
		queue.strafe_speed = m_strafe_speed;
		m_previous_tick_id = tick_id;
	}
}


