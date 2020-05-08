#include <voxen/common/player.hpp>

#include <bicycle/math.hpp>

namespace voxen
{

Player::Player() {
	updateSecondaryFactors();
}

void Player::updateState(glm::dvec3 new_pos, glm::dquat new_rot) noexcept {
	m_position = new_pos;
	m_orientation = new_rot;
	updateSecondaryFactors();
}

void Player::updateSecondaryFactors() noexcept {
	glm::dmat3 rot_mat = glm::mat3_cast(m_orientation);

	m_look_vector = bicycle::dirFromOrientation(rot_mat);
	m_right_vector = bicycle::rightFromOrientation(rot_mat);
	m_up_vector = bicycle::upFromOrientation(rot_mat);
}

}
