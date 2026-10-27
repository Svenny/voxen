#pragma once

#include <m/matrix.hpp>
#include <m/vector.hpp>

// Unit quaternions representing rotations.
//
// Components are stored as `x, y, z, w` - vector part first, scalar last, the same
// order as a `float4` in shaders. There is no four-scalar constructor (the argument
// order is a classic trap), build quaternions with named factories instead.
//
// `mul(a, b)` composes rotations like matrices do: `b` is applied first, then `a`.
// `mul(q, v)` rotates a vector, the same as `mul(toMatrix(q), v)`. Angles are in
// radians, positive rotation is counter-clockwise looking against the axis
// (right-hand rule). Default value is the identity rotation.

namespace m
{

template<typename T>
struct quaternion {
	static_assert(CFloating<T>);

	T x = T(0);
	T y = T(0);
	T z = T(0);
	T w = T(1);

	constexpr quaternion() noexcept = default;

	// Takes components as is, no normalization is done
	constexpr explicit quaternion(const vector<T, 4> &xyzw) noexcept : x(xyzw.x), y(xyzw.y), z(xyzw.z), w(xyzw.w) {}

	template<CFloating U>
	constexpr explicit quaternion(const quaternion<U> &q) noexcept : x(T(q.x)), y(T(q.y)), z(T(q.z)), w(T(q.w))
	{}

	constexpr static quaternion identity() noexcept { return {}; }

	// `axis` must be unit length
	static quaternion fromAxisAngle(const vector<T, 3> &axis, T angle) noexcept
	{
		const T half = angle / T(2);
		return quaternion(vector<T, 4>(axis * sin(half), cos(half)));
	}

	// `rot` must be a proper rotation matrix (orthonormal with determinant 1)
	constexpr static quaternion fromRotationMatrix(const matrix<T, 3, 3> &rot) noexcept
	{
		// Shepperd's method: pick the largest of `w, x, y, z` by diagonal
		// elements and derive the others from it to keep precision
		const T trace = rot[0][0] + rot[1][1] + rot[2][2];

		if (trace > T(0)) {
			const T s = sqrt(trace + T(1)) * T(2); // 4w
			return quaternion(vector<T, 4>((rot[2][1] - rot[1][2]) / s, (rot[0][2] - rot[2][0]) / s,
				(rot[1][0] - rot[0][1]) / s, s / T(4)));
		}

		if (rot[0][0] > rot[1][1] && rot[0][0] > rot[2][2]) {
			const T s = sqrt(T(1) + rot[0][0] - rot[1][1] - rot[2][2]) * T(2); // 4x
			return quaternion(vector<T, 4>(s / T(4), (rot[0][1] + rot[1][0]) / s, (rot[0][2] + rot[2][0]) / s,
				(rot[2][1] - rot[1][2]) / s));
		}

		if (rot[1][1] > rot[2][2]) {
			const T s = sqrt(T(1) + rot[1][1] - rot[0][0] - rot[2][2]) * T(2); // 4y
			return quaternion(vector<T, 4>((rot[0][1] + rot[1][0]) / s, s / T(4), (rot[1][2] + rot[2][1]) / s,
				(rot[0][2] - rot[2][0]) / s));
		}

		const T s = sqrt(T(1) + rot[2][2] - rot[0][0] - rot[1][1]) * T(2); // 4z
		return quaternion(
			vector<T, 4>((rot[0][2] + rot[2][0]) / s, (rot[1][2] + rot[2][1]) / s, s / T(4), (rot[1][0] - rot[0][1]) / s));
	}

	constexpr vector<T, 4> toVector() const noexcept { return { x, y, z, w }; }

	constexpr bool operator==(const quaternion &) const noexcept = default;
};

// Hamilton product, `b` is applied first when both are rotations
template<CFloating T>
constexpr quaternion<T> mul(const quaternion<T> &a, const quaternion<T> &b) noexcept
{
	return quaternion<T>(vector<T, 4>(a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
		a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x, a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
		a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z));
}

// Rotates a vector, `q` must be unit length
template<CFloating T>
constexpr vector<T, 3> mul(const quaternion<T> &q, const vector<T, 3> &v) noexcept
{
	// `q v q*` with the vector part factored out: v + 2w (u x v) + 2 u x (u x v)
	const vector<T, 3> u(q.x, q.y, q.z);
	const vector<T, 3> t = T(2) * cross(u, v);
	return v + q.w * t + cross(u, t);
}

template<CFloating T>
constexpr quaternion<T> conjugate(const quaternion<T> &q) noexcept
{
	return quaternion<T>(vector<T, 4>(-q.x, -q.y, -q.z, q.w));
}

template<CFloating T>
constexpr T dot(const quaternion<T> &a, const quaternion<T> &b) noexcept
{
	return dot(a.toVector(), b.toVector());
}

template<CFloating T>
constexpr T length(const quaternion<T> &q) noexcept
{
	return length(q.toVector());
}

template<CFloating T>
constexpr quaternion<T> normalize(const quaternion<T> &q) noexcept
{
	return quaternion<T>(normalize(q.toVector()));
}

// Inverse of any (not only unit) quaternion, for unit ones `conjugate` is cheaper
template<CFloating T>
constexpr quaternion<T> inverse(const quaternion<T> &q) noexcept
{
	return quaternion<T>(conjugate(q).toVector() * (T(1) / dot(q, q)));
}

// Rotation matrix with the same effect as `mul(q, v)`, `q` must be unit length
template<CFloating T>
constexpr matrix<T, 3, 3> toMatrix(const quaternion<T> &q) noexcept
{
	const T xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
	const T xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
	const T wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;

	return {
		{ T(1) - T(2) * (yy + zz), T(2) * (xy - wz), T(2) * (xz + wy) },
		{ T(2) * (xy + wz), T(1) - T(2) * (xx + zz), T(2) * (yz - wx) },
		{ T(2) * (xz - wy), T(2) * (yz + wx), T(1) - T(2) * (xx + yy) },
	};
}

// Spherical interpolation along the shortest arc, both quaternions must be unit length
template<CFloating T>
quaternion<T> slerp(const quaternion<T> &a, const quaternion<T> &b, T t) noexcept
{
	vector<T, 4> va = a.toVector();
	vector<T, 4> vb = b.toVector();

	T cos_theta = dot(va, vb);
	if (cos_theta < T(0)) {
		// `q` and `-q` are the same rotation, take the shorter arc
		vb = -vb;
		cos_theta = -cos_theta;
	}

	if (cos_theta > T(1) - T(1e-5)) {
		// Nearly parallel, `sin(theta)` is too small to divide by
		return normalize(quaternion<T>(lerp(va, vb, t)));
	}

	const T theta = std::acos(cos_theta);
	const T sin_theta = sin(theta);
	const T wa = sin((T(1) - t) * theta) / sin_theta;
	const T wb = sin(t * theta) / sin_theta;
	return quaternion<T>(va * wa + vb * wb);
}

} // namespace m
