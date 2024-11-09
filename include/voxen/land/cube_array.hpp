#pragma once

#include <glm/vec3.hpp>

#include <algorithm>

namespace voxen::land
{

template<typename T, uint32_t N>
struct CubeArray;

// View of an YXZ-ordered 3D array (three-dimensional span).
// Needed mainly to operate on sub-arrays of a `CubeArray`.
template<typename T, uint32_t N>
struct CubeArrayView {
	T *data = nullptr;
	uint32_t y_stride = 0;
	uint32_t x_stride = 0;

	T *addr(glm::uvec3 c) noexcept { return data + c.y * y_stride + c.x * x_stride + c.z; }
	const T *addr(glm::uvec3 c) const noexcept { return data + c.y * y_stride + c.x * x_stride + c.z; }

	T operator[](glm::ivec3 c) const noexcept { return *addr(glm::uvec3(c)); }
	T operator[](glm::uvec3 c) const noexcept { return *addr(c); }
	T &operator[](glm::ivec3 c) noexcept { return *addr(glm::uvec3(c)); }
	T &operator[](glm::uvec3 c) noexcept { return *addr(c); }

	T load(uint32_t x, uint32_t y, uint32_t z) const noexcept { return *(data + y * y_stride + x * x_stride + z); }

	void store(uint32_t x, uint32_t y, uint32_t z, T value) noexcept
	{
		*(data + y * y_stride + x * x_stride + z) = value;
	}

	template<uint32_t M>
	CubeArrayView<T, M> view(glm::uvec3 offset) noexcept
	{
		return CubeArrayView<T, M> { addr(offset), y_stride, x_stride };
	}

	template<uint32_t M>
	CubeArrayView<const T, M> view(glm::uvec3 offset) const noexcept
	{
		return CubeArrayView<const T, M> { addr(offset), y_stride, x_stride };
	}

	CubeArrayView<const T, N> cview() const noexcept { return CubeArrayView<const T, N> { data, y_stride, x_stride }; }

	void fill(T value) noexcept
	{
		for (uint32_t y = 0; y < N; y++) {
			for (uint32_t x = 0; x < N; x++) {
				T *ptr = data + y * y_stride + x * x_stride;
				std::fill_n(ptr, N, value);
			}
		}
	}

	void fill(glm::uvec3 begin, glm::uvec3 size, T value) noexcept
	{
		for (uint32_t y = begin.y; y < begin.y + size.y; y++) {
			for (uint32_t x = begin.x; x < begin.x + size.x; x++) {
				T *ptr = data + y * y_stride + x * x_stride + begin.z;
				std::fill_n(ptr, size.z, value);
			}
		}
	}

	void fillFrom(const CubeArrayView<const T, N> &in) noexcept
	{
		for (uint32_t y = 0; y < N; y++) {
			for (uint32_t x = 0; x < N; x++) {
				for (uint32_t z = 0; z < N; z++) {
					glm::uvec3 c(x, y, z);
					operator[](c) = in[c];
				}
			}
		}
	}

	template<uint32_t M>
	void extractTo(glm::uvec3 base, CubeArray<std::remove_const_t<T>, M> &out) const noexcept;
};

// YXZ-ordered POD 3D array with equal dimensions.
// Used to store various chunk data in "expanded" form.
template<typename T, uint32_t N>
struct CubeArray {
	static_assert(std::is_trivial_v<T>, "CubeArray supports only trivial types");

	T data[N][N][N];

	bool operator==(const CubeArray &other) const noexcept = default;

	T operator[](glm::ivec3 c) const noexcept { return data[c.y][c.x][c.z]; }
	T operator[](glm::uvec3 c) const noexcept { return data[c.y][c.x][c.z]; }
	T &operator[](glm::ivec3 c) noexcept { return data[c.y][c.x][c.z]; }
	T &operator[](glm::uvec3 c) noexcept { return data[c.y][c.x][c.z]; }

	T load(int32_t x, int32_t y, int32_t z) const noexcept { return data[y][x][z]; }
	T load(uint32_t x, uint32_t y, uint32_t z) const noexcept { return data[y][x][z]; }
	void store(int32_t x, int32_t y, int32_t z, T value) noexcept { data[y][x][z] = value; }
	void store(uint32_t x, uint32_t y, uint32_t z, T value) noexcept { data[y][x][z] = value; }

	T *begin() noexcept { return &data[0][0][0]; }
	T *end() noexcept { return &data[0][0][0] + N * N * N; }
	const T *begin() const noexcept { return &data[0][0][0]; }
	const T *end() const noexcept { return &data[0][0][0] + N * N * N; }
	size_t size() const noexcept { return N * N * N; }

	CubeArrayView<T, N> view() noexcept { return CubeArrayView<T, N> { &data[0][0][0], N * N, N }; }
	CubeArrayView<const T, N> view() const noexcept { return CubeArrayView<const T, N> { &data[0][0][0], N * N, N }; }
	CubeArrayView<const T, N> cview() const noexcept { return CubeArrayView<const T, N> { &data[0][0][0], N * N, N }; }

	template<uint32_t M>
	CubeArrayView<T, M> view(glm::uvec3 offset) noexcept
	{
		return CubeArrayView<T, M> { &data[offset.y][offset.x][offset.z], N * N, N };
	}

	template<uint32_t M>
	CubeArrayView<const T, M> view(glm::uvec3 offset) const noexcept
	{
		return CubeArrayView<const T, M> { &data[offset.y][offset.x][offset.z], N * N, N };
	}

	void fill(T value) noexcept { std::fill_n(&data[0][0][0], N * N * N, value); }

	void fill(glm::uvec3 begin, glm::uvec3 size, T value) noexcept
	{
		for (uint32_t y = begin.y; y < begin.y + size.y; y++) {
			for (uint32_t x = begin.x; x < begin.x + size.x; x++) {
				std::fill_n(data[y][x] + begin.z, size.z, value);
			}
		}
	}

	template<uint32_t M>
	void extractTo(glm::uvec3 base, CubeArray<T, M> &out) const noexcept
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

	template<uint32_t M>
	void insertFrom(glm::uvec3 base, const CubeArray<T, M> &in) noexcept
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

template<typename T, uint32_t N>
template<uint32_t M>
void CubeArrayView<T, N>::extractTo(glm::uvec3 base, CubeArray<std::remove_const_t<T>, M> &out) const noexcept
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

} // namespace voxen::land
