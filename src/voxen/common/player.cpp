#include <voxen/common/player.hpp>

#include <extras/math.hpp>

namespace voxen
{

Player::Player() : m_position(-1500, 250, -1100)
{
	updateSecondaryFactors();
}

void Player::updateState(glm::dvec3 new_pos, glm::dquat new_rot) noexcept
{
	m_position = new_pos;
	m_orientation = new_rot;
	updateSecondaryFactors();
}

void Player::updateSecondaryFactors() noexcept
{
	glm::dmat3 rot_mat = glm::mat3_cast(m_orientation);

	m_look_vector = extras::dirFromOrientation(rot_mat);
	m_right_vector = extras::rightFromOrientation(rot_mat);
	m_up_vector = extras::upFromOrientation(rot_mat);
}

} // namespace voxen
