#include <voxen/common/player.hpp>

namespace voxen
{

Player::Player() : m_position(-1500, 250, -1100)
{
	updateSecondaryFactors();
}

void Player::updateState(m::double3 new_pos, m::dquat new_rot) noexcept
{
	m_position = new_pos;
	m_orientation = new_rot;
	updateSecondaryFactors();
}

void Player::updateSecondaryFactors() noexcept
{
	// Orientation rotates world space into camera space,
	// so camera axes expressed in world space are the matrix rows
	const m::double3x3 rot_mat = m::toMatrix(m_orientation);

	m_right_vector = rot_mat[0];
	m_up_vector = rot_mat[1];
	m_look_vector = rot_mat[2];
}

} // namespace voxen
