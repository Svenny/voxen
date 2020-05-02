#include <voxen/common/gameview.hpp>

#include <voxen/common/world.hpp>
#include <voxen/common/player.hpp>
#include <GLFW/glfw3.h>

#include <voxen/util/log.hpp>

using namespace voxen;

const static float kDefaultFov = 65.0f;
const static float kDefaultAspectRatio = 16.0f / 9.0f;

const static float kDefaultMouseSensitivity = 0.5f;
const static float kDefaultForwardSpeed = 50.0f;
const static float kDefaultStrafeSpeed = 25.0f;

const static float kRollSpeed = 1.5f;

GameView::GameView () :
	m_proj (1.0f), m_view (1.0f), m_position (0.0f), m_orientation (1.0f, 0.0f, 0.0f, 0.0f),
	m_fovDegrees (kDefaultFov), m_tanAX2 (tanf (kDefaultFov * 0.5f)), m_tanAY2 (m_tanAX2 / kDefaultAspectRatio),
	m_width (kDefaultAspectRatio), m_height (1.0f), m_mouseSensitivity (kDefaultMouseSensitivity), m_forwardSpeed (kDefaultForwardSpeed),
	m_strafeSpeed (kDefaultStrafeSpeed) {
	for (int i = 0; i < Key::KeyCount; i++)
		m_keyPressed[i] = false;
}

void voxen::GameView::init(const voxen::Player& player)
{
	m_position = player.position();
	m_proj = player.projectionMatrix(); // TODO real init projection
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

bool voxen::GameView::handleCursor(double xpos, double ypos) noexcept
{
	double dx = xpos - m_prev_xpos;
	double dy = ypos - m_prev_ypos;
	m_prev_xpos = xpos;
	m_prev_ypos = ypos;

	(void)dx;
	(void)dy;

	// TODO Actual code
	/*
	float yawAngle = m_mouseSensitivity * atanf (2 * dx * m_tanAX2 / m_width);
	float pitchAngle = m_mouseSensitivity * atanf (2 * dy * m_tanAY2 / m_height);
	auto rotQuat = quatFromEulerAngles (pitchAngle, yawAngle, 0.0f);
	m_orientation = glm::normalize (rotQuat * m_orientation);

	m_view = glm::mat4_cast (m_orientation) * glm::translate (glm::mat4 (1.0f), -m_position);
	*/
	return true;
}


void GameView::resetKeyState() noexcept {
	for (size_t i = 0; i < Key::KeyCount; i++)
		m_keyPressed[i] = false;
}

/*
static glm::quat quatFromEulerAngles (float pitch, float yaw, float roll) noexcept {
	glm::vec3 eulerAngle (pitch * 0.5f, yaw * 0.5f, roll * 0.5f);
	glm::vec3 c = glm::cos (eulerAngle * 0.5f);
	glm::vec3 s = glm::sin (eulerAngle * 0.5f);

	glm::quat res;
	res.w = c.x * c.y * c.z + s.x * s.y * s.z;
	res.x = s.x * c.y * c.z - c.x * s.y * s.z;
	res.y = c.x * s.y * c.z + s.x * c.y * s.z;
	res.z = c.x * c.y * s.z - s.x * s.y * c.z;
	return res;
}
*/

void GameView::update (const Player& player, DebugQueueRtW& queue) noexcept {
	(void)player; // TODO Player needs for orientation code

	glm::vec3 move_forward_direction{0.0f};
	glm::vec3 move_strafe_direction{0.0f};

	auto rotMat = glm::mat3_cast (m_orientation);
	glm::vec3 dir (rotMat[0][2], rotMat[1][2], rotMat[2][2]);
	glm::vec3 right (rotMat[0][0], rotMat[1][0], rotMat[2][0]);
	glm::vec3 up (rotMat[0][1], rotMat[1][1], rotMat[2][1]);

	float dx = 0;
	if (m_keyPressed[Key::KeyLeft]) dx -= 1;
	if (m_keyPressed[Key::KeyRight]) dx += 1;
	move_strafe_direction += dx * right;

	float dy = 0;
	if (m_keyPressed[Key::KeyDown]) dy -= 1;
	if (m_keyPressed[Key::KeyUp]) dy += 1;
	move_strafe_direction += dy * up;

	float dz = 0;
	if (m_keyPressed[Key::KeyBack]) dz -= 1;
	if (m_keyPressed[Key::KeyForward]) dz += 1;
	move_forward_direction += dz * dir;

	/*
	float rollAngle = 0;
	if (m_keyPressed[Key::KeyRollCCW]) rollAngle -= kRollSpeed * deltaTime;
	if (m_keyPressed[Key::KeyRollCW]) rollAngle += kRollSpeed * deltaTime;
	auto rotQuat = quatFromEulerAngles (0, 0, rollAngle);
	m_orientation = glm::normalize (rotQuat * m_orientation);

	m_view = glm::mat4_cast (m_orientation) * glm::translate (glm::mat4 (1.0f), -m_position);
	*/

	queue.player_forward_movement_direction = move_forward_direction;
	queue.player_strafe_movement_direction = move_strafe_direction;
	queue.forward_speed = m_forwardSpeed;
	queue.strafe_speed = m_strafeSpeed;
}


