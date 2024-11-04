#include <voxen/util/concentric_octahedra_walker.hpp>

#include <cmath>

namespace voxen
{

static_assert(sizeof(ConcentricOctahedraWalker) == sizeof(uint32_t), "ConcentricOctahedraWalker packing is broken");

glm::ivec3 ConcentricOctahedraWalker::step() noexcept
{
	glm::ivec3 result;

	int32_t radius = m_radius;
	int32_t dx = m_dx;
	int32_t dz = m_dz;

	int32_t dy = radius - std::abs(dx) - std::abs(dz);
	result = { dx, m_dy_negative ? -dy : dy, dz };

	if (m_dy_negative || dy == 0) {
		m_dy_negative = 0;

		dz++;
		if (dz > radius - std::abs(dx)) {
			dx++;
			if (dx > radius) {
				radius++;
				if (radius > m_max_radius) {
					radius = 0;
					m_wrapped_around = 1;
				}

				dx = -radius;
			}

			dz = -(radius - std::abs(dx));
		}
	} else {
		m_dy_negative = 1;
	}

	m_radius = uint8_t(radius);
	m_dx = int8_t(dx);
	m_dz = int8_t(dz);

	return result;
}

} // namespace voxen
