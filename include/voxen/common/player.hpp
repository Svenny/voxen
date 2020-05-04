#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace voxen
{

class Player {
public:
	glm::dvec3 position() const noexcept { return m_position; }
	glm::dquat orientation() const noexcept { return m_orientation; }

	void updateState(glm::dvec3 new_pos, glm::dquat new_rot) noexcept;

	glm::dvec3 lookVector() const noexcept { return m_look_vector; }
	glm::dvec3 upVector() const noexcept { return m_up_vector; }
	glm::dvec3 rightVector() const noexcept { return m_right_vector; }

	glm::mat4 projectionMatrix() const noexcept { return m_proj_matrix; }
	glm::mat4 viewMatrix() const noexcept { return m_view_matrix; }
	glm::mat4 cameraMatrix() const noexcept { return m_cam_matrix; }
private:
	glm::dvec3 m_position { 100, 100, -1100 };
	glm::dquat m_orientation = glm::identity<glm::dquat>();

	double fovx = 1.5, fovy = 1.5 * 9.0 / 16.0;
	double znear = 0.1, zfar = 1'000'000.0;

	glm::dvec3 m_look_vector { 0, 0, 1 };
	glm::dvec3 m_up_vector { 0, 1, 0 };
	glm::dvec3 m_right_vector { 1, 0, 0 };

	glm::mat4 m_proj_matrix;
	glm::mat4 m_view_matrix;
	glm::mat4 m_cam_matrix;

	void updateSecondaryFactors() noexcept;
};

}
