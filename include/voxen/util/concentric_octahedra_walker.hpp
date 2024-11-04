#pragma once

#include <voxen/visibility.hpp>

#include <glm/vec3.hpp>

namespace voxen
{

// Helper to visit a set of concentric octahedra (spheres in Manhattan metric space)
// in order of increasing radius. In 2D (side projection) it looks like this:
//
//    @
//   @#@    Visit order:
//  @#*#@   1 -> 0, center point (0, 0, 0)
// @#*0*#@  2 -> * (radius 1)
//  @#*#@   3 -> # (radius 2)
//   @#@    4 -> @ (radius 3)
//    @
//
// Center point is always at origin, i.e. the first call to `step()` returns (0, 0, 0).
// The next 6 calls to `step()` return vectors with one +-1 and two zeros, and so on.
//
// Points of a single radius are visited in an unspecified but fixed order.
//
// State is very small, packing in just 4 bytes to allow keeping many walkers.
class VOXEN_API ConcentricOctahedraWalker {
public:
	// Default constructor as if `max_radius` was 0
	ConcentricOctahedraWalker() = default;
	// `max_radius` must be less than 128
	explicit ConcentricOctahedraWalker(uint8_t max_radius) noexcept : m_max_radius(max_radius) {}

	// Do one walk step and return its offset.
	// If this was the last offset before repeating starts,
	// `wrappedAround()` will return true after this call.
	glm::ivec3 step() noexcept;
	// Returns true when `step()` calls have returned all possible
	// offsets for this `max_radius` and now are repeating values.
	bool wrappedAround() const noexcept { return m_wrapped_around; }

private:
	uint8_t m_max_radius : 7 = 0;
	uint8_t m_wrapped_around : 1 = 0;
	uint8_t m_radius : 7 = 0;
	uint8_t m_dy_negative : 1 = 0;
	int8_t m_dx = 0;
	int8_t m_dz = 0;
};

} // namespace voxen
