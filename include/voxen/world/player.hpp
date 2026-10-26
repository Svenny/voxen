#pragma once

#include <m/quaternion.hpp>
#include <m/vector.hpp>

namespace voxen::world
{

class Player {
public:
	Player();

	const m::double3 &position() const noexcept { return m_position; }
	const m::dquat &orientation() const noexcept { return m_orientation; }

	void updateState(m::double3 new_pos, m::dquat new_rot) noexcept;

	const m::double3 &lookVector() const noexcept { return m_look_vector; }
	const m::double3 &upVector() const noexcept { return m_up_vector; }
	const m::double3 &rightVector() const noexcept { return m_right_vector; }

private:
	m::double3 m_position;
	m::dquat m_orientation;

	m::double3 m_look_vector;
	m::double3 m_up_vector;
	m::double3 m_right_vector;

	void updateSecondaryFactors() noexcept;
};

} // namespace voxen::world
