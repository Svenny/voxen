#pragma once

#include <m/matrix.hpp>
#include <m/quaternion.hpp>
#include <m/vector.hpp>

// Transform builders and the spatial conventions of the engine.
//
// All spaces are right-handed with Y up:
// - World: X east, Y up, Z south, distances in metres.
// - Camera (view): X right, Y up, looking along -Z, like glTF cameras. An orientation
//   quaternion rotates camera space into world space, so the camera's forward direction
//   in world space is `mul(orientation, float3(0, 0, -1))` and `viewFromCamera` makes
//   the world-to-view matrix.
// - Clip: Vulkan's, X right, Y down, depth 0..1. Projections built here are reversed-Z
//   with an infinite far plane: the near plane maps to depth 1 and infinity to 0, so
//   depth is cleared to 0 and tested with "greater". They also flip Y, the viewport
//   stays non-flipped.
// - Front faces are counter-clockwise (right-hand rule, normal towards the viewer),
//   with the above they rasterize as `VK_FRONT_FACE_COUNTER_CLOCKWISE`.
//
// Affine transforms are `matrix<T, 3, 4>` with the `0 0 0 1` row implied, `mul()`
// composes them and `homogeneous()` extends one to 4x4 to combine with projections.

namespace m
{

template<CFloating T>
constexpr matrix<T, 3, 4> translation(const vector<T, 3> &t) noexcept
{
	return {
		{ T(1), T(0), T(0), t.x },
		{ T(0), T(1), T(0), t.y },
		{ T(0), T(0), T(1), t.z },
	};
}

template<CFloating T>
constexpr matrix<T, 3, 4> scaling(const vector<T, 3> &s) noexcept
{
	return {
		{ s.x, T(0), T(0), T(0) },
		{ T(0), s.y, T(0), T(0) },
		{ T(0), T(0), s.z, T(0) },
	};
}

// `q` must be unit length
template<CFloating T>
constexpr matrix<T, 3, 4> rotation(const quaternion<T> &q) noexcept
{
	const matrix<T, 3, 3> r = toMatrix(q);
	return { { r[0], T(0) }, { r[1], T(0) }, { r[2], T(0) } };
}

// Places an object: rotates by `orientation`, then translates to `position`.
// For a camera this is the view-to-world matrix.
template<CFloating T>
constexpr matrix<T, 3, 4> rigidTransform(const quaternion<T> &orientation, const vector<T, 3> &position) noexcept
{
	const matrix<T, 3, 3> r = toMatrix(orientation);
	return { { r[0], position.x }, { r[1], position.y }, { r[2], position.z } };
}

// World-to-view matrix of a camera with the given `orientation` at `position`,
// the inverse of `rigidTransform` with the same arguments
template<CFloating T>
constexpr matrix<T, 3, 4> viewFromCamera(const quaternion<T> &orientation, const vector<T, 3> &position) noexcept
{
	// Inverse of a rotation is its transpose
	const matrix<T, 3, 3> r = transpose(toMatrix(orientation));
	return {
		{ r[0], -dot(r[0], position) },
		{ r[1], -dot(r[1], position) },
		{ r[2], -dot(r[2], position) },
	};
}

// Extends an affine transform with its implied `0 0 0 1` row
template<CFloating T>
constexpr matrix<T, 4, 4> homogeneous(const matrix<T, 3, 4> &a) noexcept
{
	return { a[0], a[1], a[2], { T(0), T(0), T(0), T(1) } };
}

// View-to-clip matrix, see the header comment for its properties.
// `fov_y` is the vertical field of view in radians, `aspect` is width / height.
template<CFloating T>
matrix<T, 4, 4> perspectiveInfinite(T fov_y, T aspect, T z_near) noexcept
{
	const T fy = T(1) / tan(fov_y / T(2));
	const T fx = fy / aspect;
	// Depth is `z_near / -z_view`, clip W is `-z_view` so that near maps to 1
	return {
		{ fx, T(0), T(0), T(0) },
		{ T(0), -fy, T(0), T(0) },
		{ T(0), T(0), T(0), z_near },
		{ T(0), T(0), T(-1), T(0) },
	};
}

} // namespace m
