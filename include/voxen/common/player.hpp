#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace voxen
{

class Player {
public:
	Player();

	const glm::dvec3 &position() const noexcept { return m_position; }
	const glm::dquat &orientation() const noexcept { return m_orientation; }

	void updateState(glm::dvec3 new_pos, glm::dquat new_rot) noexcept;

	const glm::dvec3 &lookVector() const noexcept { return m_look_vector; }
	const glm::dvec3 &upVector() const noexcept { return m_up_vector; }
	const glm::dvec3 &rightVector() const noexcept { return m_right_vector; }

private:
	glm::dvec3 m_position;
	glm::dquat m_orientation = glm::identity<glm::dquat>();

	glm::dvec3 m_look_vector;
	glm::dvec3 m_up_vector;
	glm::dvec3 m_right_vector;

	void updateSecondaryFactors() noexcept;
};

} // namespace voxen
