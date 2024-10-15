#pragma once

#include <voxen/visibility.hpp>

#include <glm/vec3.hpp>

namespace voxen
{

class VOXEN_API ConcentricOctahedronWalker {
public:
	ConcentricOctahedronWalker() = default;
	explicit ConcentricOctahedronWalker(uint16_t max_radius) noexcept : m_max_radius(max_radius) {}

	glm::ivec3 step() noexcept;
	bool wrappedAround() const noexcept { return m_wrapped_around; }

private:
	uint16_t m_max_radius : 15 = 0;
	uint16_t m_wrapped_around : 1 = 0;
	uint16_t m_radius : 15 = 0;
	uint16_t m_dy_negative : 1 = 0;
	int16_t m_dx = 0;
	int16_t m_dz = 0;
};

} // namespace voxen
