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
	// Orientation rotates camera space into world space, camera looks along -Z
	m_right_vector = m::mul(m_orientation, m::double3(1, 0, 0));
	m_up_vector = m::mul(m_orientation, m::double3(0, 1, 0));
	m_look_vector = m::mul(m_orientation, m::double3(0, 0, -1));
}

} // namespace voxen
