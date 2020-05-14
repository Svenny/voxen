#include <voxen/common/gameview.hpp>

#include <voxen/common/world.hpp>
#include <voxen/common/player.hpp>
#include <GLFW/glfw3.h>

#include <voxen/util/log.hpp>

#include <glm/gtc/quaternion.hpp>

using namespace voxen;

const static float kDefaultMouseSensitivity = 1.5f;
const static float kDefaultForwardSpeed = 100.0f;
const static float kDefaultStrafeSpeed = 50.0f;

const static float kRollSpeed = 1.5f * 0.01f;

GameView::GameView (Window& window):
	m_mouseSensitivity(kDefaultMouseSensitivity), m_forwardSpeed(kDefaultForwardSpeed), m_strafeSpeed(kDefaultStrafeSpeed),
	m_previous_tick_id(-1), m_window(&window), m_is_got_left_mouse_click(false), m_is_used_orientation_cursor(false) {
	m_width = window.width();
	m_height = window.height();
	std::pair<double, double> pos = window.cursorPos();
	m_prev_xpos = pos.first;
	m_prev_ypos = pos.second;

	for (int i = 0; i < Key::KeyCount; i++)
		m_keyPressed[i] = false;
}

void voxen::GameView::init(const voxen::Player& player) noexcept {
	m_player_dir = player.lookVector();
	m_player_up = player.upVector();
	m_player_right = player.rightVector();
	m_orientation = player.orientation();
	m_proj_matrix = extras::perspective(m_fov_x, m_fov_y, m_z_near, m_z_far);
	m_view_matrix = extras::lookAt(player.position(), m_player_dir, m_player_up);
	m_cam_matrix = m_proj_matrix * m_view_matrix;
	resetKeyState();
}

bool GameView::handleKey(int key, int scancode, int action, int mods) noexcept {
	(void)scancode;
	if (mods == 0) {
		if (key == GLFW_KEY_W) {
			m_keyPressed[GameView::KeyForward] = action != GLFW_RELEASE;
			return true;
		} else if (key == GLFW_KEY_D) {
			m_keyPressed[Key::KeyRight] = action != GLFW_RELEASE;
			return true;
		} else if (key == GLFW_KEY_A) {
			m_keyPressed[Key::KeyLeft] = action != GLFW_RELEASE;
			return true;
		} else if (key == GLFW_KEY_S) {
			m_keyPressed[Key::KeyBack] = action != GLFW_RELEASE;
			return true;
		} else if (key == GLFW_KEY_C) {
			m_keyPressed[Key::KeyDown] = action != GLFW_RELEASE;
			return true;
		} else if (key == GLFW_KEY_SPACE) {
			m_keyPressed[Key::KeyUp] = action != GLFW_RELEASE;
			return true;
		} else if (key == GLFW_KEY_Q) {
			m_keyPressed[Key::KeyRollCCW] = action != GLFW_RELEASE;
			return true;
		} else if (key == GLFW_KEY_E) {
			m_keyPressed[Key::KeyRollCW] = action != GLFW_RELEASE;
			return true;
		}
	}
	return false;
}

bool GameView::handleMouseScroll(double xoffset, double yoffset) noexcept
{
	(void)xoffset;
	if (yoffset > 0)
	{
		m_strafeSpeed = 1.1 * m_strafeSpeed;
		m_forwardSpeed = 1.1 * m_forwardSpeed;
	}
	else
	{
		m_strafeSpeed = 0.9 * m_strafeSpeed;
		m_forwardSpeed = 0.9 * m_forwardSpeed;
	}
	return true;
}

bool voxen::GameView::handleMouseKey(int button, int action, int mods) noexcept
{
	if (mods == 0) {
		if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
			m_is_got_left_mouse_click = true;
			return true;
		}
	}
	return false;
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
	for (size_t i = 0; i < Key::KeyCount; i++)
		m_keyPressed[i] = false;
}

void GameView::update (const Player& player, DebugQueueRtW& queue, uint64_t tick_id) noexcept {
	if (m_is_used_orientation_cursor) {
		if (m_is_got_left_mouse_click) {
			m_window->useRegularCursor();
			m_is_used_orientation_cursor = false;
			m_is_got_left_mouse_click = false;
			return;
		}

		glm::dvec3 move_forward_direction{0.0};
		glm::dvec3 move_strafe_direction{0.0};

		if (m_previous_tick_id < tick_id) {
			m_player_dir = player.lookVector();
			m_player_up = player.upVector();
			m_player_right = player.rightVector();
			m_orientation = player.orientation();
		}

		double dx = 0;
		if (m_keyPressed[Key::KeyLeft]) dx -= 1;
		if (m_keyPressed[Key::KeyRight]) dx += 1;
		move_strafe_direction += dx * m_player_right;

		double dy = 0;
		if (m_keyPressed[Key::KeyDown]) dy -= 1;
		if (m_keyPressed[Key::KeyUp]) dy += 1;
		move_strafe_direction += dy * m_player_up;

		double dz = 0;
		if (m_keyPressed[Key::KeyBack]) dz -= 1;
		if (m_keyPressed[Key::KeyForward]) dz += 1;
		move_forward_direction += dz * m_player_dir;

		dx = (m_prev_xpos - m_newest_xpos) * m_mouseSensitivity;
		m_prev_xpos = m_newest_xpos;
		double tan_half_fovx = std::tan(m_fov_x * 0.5);
		double yawAngle = atan (2 * dx * tan_half_fovx / m_width);

		dy = (m_prev_ypos - m_newest_ypos) * m_mouseSensitivity;
		m_prev_ypos = m_newest_ypos;
		double tan_half_fovy = std::tan(m_fov_y * 0.5);
		double pitchAngle = atan (2 * dy * tan_half_fovy / m_height);

		double rollAngle = 0;
		if (m_keyPressed[Key::KeyRollCCW]) rollAngle -= kRollSpeed;
		if (m_keyPressed[Key::KeyRollCW]) rollAngle += kRollSpeed;

		auto rotQuat = quatFromEulerAngles(pitchAngle, yawAngle, rollAngle);
		m_orientation = glm::normalize(rotQuat * m_orientation);

		glm::dmat3 rot_mat = glm::mat3_cast(m_orientation);
		m_player_dir = extras::dirFromOrientation(rot_mat);
		m_player_right = extras::rightFromOrientation(rot_mat);
		m_player_up = extras::upFromOrientation(rot_mat);

		m_proj_matrix = extras::perspective(m_fov_x, m_fov_y, m_z_near, m_z_far);
		m_view_matrix = extras::lookAt(player.position(), m_player_dir, m_player_up);
		m_cam_matrix = m_proj_matrix * m_view_matrix;

		queue.player_forward_movement_direction = move_forward_direction;
		queue.player_strafe_movement_direction = move_strafe_direction;
		queue.player_orientation = m_orientation;
		queue.forward_speed = m_forwardSpeed;
		queue.strafe_speed = m_strafeSpeed;
		m_previous_tick_id = tick_id;
	} else {
		if (m_is_got_left_mouse_click) {
			m_window->useOrientationCursor();
			m_is_used_orientation_cursor = true;
			m_is_got_left_mouse_click = false;

			std::pair<double, double> pos = m_window->cursorPos();
			m_prev_xpos = pos.first;
			m_prev_ypos = pos.second;
		}
	}
}


