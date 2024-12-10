#pragma once

#include <glm/vec3.hpp>

#include <cstdint>
#include <type_traits>

namespace voxen::land::Utils
{

// Visit all points in [0; N)^3 space in YXZ order, calling F(x, y, z)
template<uint32_t N, typename F>
inline void forYXZ(F &&fn) noexcept(std::is_nothrow_invocable_v<F, uint32_t, uint32_t, uint32_t>)
{
	for (uint32_t y = 0; y < N; y++) {
		for (uint32_t x = 0; x < N; x++) {
			for (uint32_t z = 0; z < N; z++) {
				fn(x, y, z);
			}
		}
	}
}

// Offset `base` coordinate by `step` as appropriate for a chunk "child" key in standard YXZ order.
// `child_id` must be in range [0; 8), otherwise behavior is undefined.
// XXX: ensure this is compiled to branchless asm, might as well rewrite in SSE (it's just masked int32 adds)
inline glm::ivec3 offsetChildCoord(size_t child_id, glm::ivec3 base, int32_t step) noexcept
{
	if (child_id & 0b100) {
		base.y += step;
	}

	if (child_id & 0b010) {
		base.x += step;
	}

	if (child_id & 0b001) {
		base.z += step;
	}

	return base;
}

} // namespace voxen::land::Utils
