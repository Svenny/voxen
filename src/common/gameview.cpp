#include <voxen/common/gameview.hpp>

#include <voxen/common/world.hpp>
#include <voxen/common/player.hpp>
#include <GLFW/glfw3.h>

#include <voxen/util/log.hpp>

using namespace voxen;

const static float kDefaultFov = 65.0f;
const static float kDefaultAspectRatio = 16.0f / 9.0f;

const static float kDefaultMouseSensitivity = 1.5f;
const static float kDefaultForwardSpeed = 50.0f;
const static float kDefaultStrafeSpeed = 25.0f;

const static float kRollSpeed = 1.5f;

GameView::GameView () :
	m_fovDegrees (kDefaultFov), m_tanAX2 (tanf (kDefaultFov * 0.5f)), m_tanAY2 (m_tanAX2 / kDefaultAspectRatio),
	m_width (kDefaultAspectRatio), m_height (1.0f), m_mouseSensitivity (kDefaultMouseSensitivity), m_forwardSpeed (kDefaultForwardSpeed),
	m_strafeSpeed (kDefaultStrafeSpeed) {
	for (int i = 0; i < Key::KeyCount; i++)
		m_keyPressed[i] = false;
}

void voxen::GameView::init(const voxen::Player& player)
{
	(void)player;
	resetKeyState();
}

bool GameView::handleKey(int key, int scancode, int action, int mods) noexcept {
	(void)scancode;
	(void)mods;
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
	(void)button;
	(void)action;
	(void)mods;
	return false;
}

static glm::dquat quatFromEulerAngles (double pitch, double yaw, double roll) noexcept {
	glm::dvec3 eulerAngle (pitch, yaw, roll);
	glm::dvec3 c = glm::cos (eulerAngle * 0.25);
	glm::dvec3 s = glm::sin (eulerAngle * 0.25);

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

void GameView::update (const Player& player, DebugQueueRtW& queue) noexcept {
	glm::dvec3 move_forward_direction{0.0};
	glm::dvec3 move_strafe_direction{0.0};

	glm::dvec3 dir = player.lookVector();
	glm::dvec3 right = player.rightVector();
	glm::dvec3 up = player.upVector();

	double dx = 0;
	if (m_keyPressed[Key::KeyLeft]) dx -= 1;
	if (m_keyPressed[Key::KeyRight]) dx += 1;
	move_strafe_direction += dx * right;

	double dy = 0;
	if (m_keyPressed[Key::KeyDown]) dy -= 1;
	if (m_keyPressed[Key::KeyUp]) dy += 1;
	move_strafe_direction += dy * up;

	double dz = 0;
	if (m_keyPressed[Key::KeyBack]) dz -= 1;
	if (m_keyPressed[Key::KeyForward]) dz += 1;
	move_forward_direction += dz * dir;

	dx = m_prev_xpos - m_newest_xpos;
	dy = m_prev_ypos - m_newest_ypos;
	m_prev_xpos = m_newest_xpos;
	m_prev_ypos = m_newest_ypos;

	double yawAngle = m_mouseSensitivity * atan (2 * dx * m_tanAX2 / m_width);
	double pitchAngle = m_mouseSensitivity * atan (2 * dy * m_tanAY2 / m_height);
	double rollAngle = 0;
	if (m_keyPressed[Key::KeyRollCCW]) rollAngle -= kRollSpeed;
	if (m_keyPressed[Key::KeyRollCW]) rollAngle += kRollSpeed;
	auto rotQuat = quatFromEulerAngles (pitchAngle, yawAngle, rollAngle);

	queue.player_forward_movement_direction = move_forward_direction;
	queue.player_strafe_movement_direction = move_strafe_direction;
	queue.player_rotation_quat = rotQuat;
	queue.forward_speed = m_forwardSpeed;
	queue.strafe_speed = m_strafeSpeed;
}


