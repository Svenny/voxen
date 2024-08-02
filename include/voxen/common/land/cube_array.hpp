#pragma once

#include <glm/vec3.hpp>

#include <algorithm>

namespace voxen::land
{

// YXZ-ordered POD 3D array with equal dimensions.
// Used to store various chunk data in "expanded" form.
template<typename T, size_t N>
struct CubeArray {
	static_assert(std::is_trivial_v<T>, "CubeArray supports only trivial types");

	T data[N][N][N];

	T operator[](glm::ivec3 c) const noexcept { return data[c.y][c.x][c.z]; }
	T operator[](glm::uvec3 c) const noexcept { return data[c.y][c.x][c.z]; }
	T &operator[](glm::ivec3 c) noexcept { return data[c.y][c.x][c.z]; }
	T &operator[](glm::uvec3 c) noexcept { return data[c.y][c.x][c.z]; }

	const T *begin() const noexcept { return &data[0][0][0]; }
	const T *end() const noexcept { return &data[0][0][0] + N * N * N; }
	size_t size() const noexcept { return N * N * N; }

	void fill(T value) noexcept { std::fill_n(&data[0][0][0], N * N * N, value); }

	void fill(glm::uvec3 begin, glm::uvec3 size, T value) noexcept
	{
		for (uint32_t y = begin.y; y < begin.y + size.y; y++) {
			for (uint32_t x = begin.x; x < begin.x + size.x; x++) {
				std::fill_n(data[y][x] + begin.z, size.z, value);
			}
		}
	}

	template<size_t M>
	void gather(glm::uvec3 base, CubeArray<T, M> &out) const noexcept
	{
		static_assert(M <= N);
		for (uint32_t y = 0; y < M; y++) {
			for (uint32_t x = 0; x < M; x++) {
				for (uint32_t z = 0; z < M; z++) {
					glm::uvec3 c(x, y, z);
					out[c] = operator[](base + c);
				}
			}
		}
	}

	template<size_t M>
	void scatter(glm::uvec3 base, const CubeArray<T, M> &in) noexcept
	{
		static_assert(M <= N);
		for (uint32_t y = 0; y < M; y++) {
			for (uint32_t x = 0; x < M; x++) {
				for (uint32_t z = 0; z < M; z++) {
					glm::uvec3 c(x, y, z);
					operator[](base + c) = in[c];
				}
			}
		}
	}
};

} // namespace voxen::land
