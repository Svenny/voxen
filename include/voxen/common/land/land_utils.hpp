#pragma once

#include <glm/vec3.hpp>

#include <cstdint>
#include <type_traits>

namespace voxen::land
{

class Utils {
public:
	Utils() = delete;

	template<uint32_t N, typename F>
	static void forYXZ(F &&fn) noexcept(std::is_nothrow_invocable_v<F, uint32_t, uint32_t, uint32_t>)
	{
		for (uint32_t y = 0; y < N; y++) {
			for (uint32_t x = 0; x < N; x++) {
				for (uint32_t z = 0; z < N; z++) {
					fn(x, y, z);
				}
			}
		}
	}
};

} // namespace voxen::land
