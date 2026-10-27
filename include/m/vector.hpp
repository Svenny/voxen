#pragma once

#include <m/fwd.hpp>

#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <type_traits>

// Small vectors of arithmetic components and functions on them, the core of `m`.
//
// Naming and semantics follow HLSL where it has an equivalent (`float3`, `lerp`,
// `saturate`, `frac`...), GLSL names fill the gaps (`lessThan`, `equal`...) and
// anything else is our own. Deliberate differences from HLSL:
// - `operator==` compares whole vectors and returns `bool`, `equal()` is componentwise
// - there are no relational operators, use `lessThan()` and friends with `all()`/`any()`
// - conversions between component types are explicit, and so is shrinking: `xy()`/`xyz()`
// - `round()` rounds halfway cases to even (like HLSL `round`, unlike `std::round`)
// - NaN is a precondition violation everywhere, nothing checks for it. `min`/`max`,
//   `clamp` and `saturate` propagate a NaN `x` (HLSL `saturate` maps it to 0) and
//   packing it into an integer is undefined behaviour.
//
// Vectors are zero-initialized by default, trivially copyable and tightly packed:
// `sizeof(vector<T, N>) == N * sizeof(T)` and `alignof(vector<T, N>) == alignof(T)`.
// This is a guarantee, they are safe in structs shared with GPU (scalar block layout).
//
// Nothing here uses SIMD. Functions can get arch-specific paths later behind
// `if !consteval`, constant evaluation always takes the generic code.
//
// Everything is `constexpr` except trigonometry. Constant evaluation of `sqrt` (and
// so `length`, `normalize`...) can differ from the runtime result by one ulp, and that
// of `lerp` by the rounding of its product, see there.

namespace m
{

// Any arithmetic type but `bool`, which has only logical operations
template<typename T>
concept CScalar = std::is_arithmetic_v<T> && !std::same_as<T, bool>;

template<typename T>
concept CFloating = std::floating_point<T>;

template<typename T>
concept CIntegral = std::integral<T> && !std::same_as<T, bool>;

// Anything that can be a vector component
template<typename T>
concept CComponent = std::is_arithmetic_v<T>;

template<CComponent T>
struct vector<T, 2> {
	T x = T(0);
	T y = T(0);

	constexpr vector() noexcept = default;
	constexpr explicit vector(T s) noexcept : x(s), y(s) {}
	constexpr vector(T x, T y) noexcept : x(x), y(y) {}

	template<CComponent U>
	constexpr explicit vector(const vector<U, 2> &v) noexcept : x(T(v.x)), y(T(v.y))
	{}

	constexpr T &operator[](size_t i) noexcept
	{
		assert(i < 2);
		return this->*MEMBERS[i];
	}

	constexpr const T &operator[](size_t i) const noexcept
	{
		assert(i < 2);
		return this->*MEMBERS[i];
	}

	constexpr bool operator==(const vector &) const noexcept = default;

private:
	constexpr static T vector::*MEMBERS[2] = { &vector::x, &vector::y };
};

template<CComponent T>
struct vector<T, 3> {
	T x = T(0);
	T y = T(0);
	T z = T(0);

	constexpr vector() noexcept = default;
	constexpr explicit vector(T s) noexcept : x(s), y(s), z(s) {}
	constexpr vector(T x, T y, T z) noexcept : x(x), y(y), z(z) {}
	constexpr vector(const vector<T, 2> &xy, T z) noexcept : x(xy.x), y(xy.y), z(z) {}

	template<CComponent U>
	constexpr explicit vector(const vector<U, 3> &v) noexcept : x(T(v.x)), y(T(v.y)), z(T(v.z))
	{}

	constexpr vector<T, 2> xy() const noexcept { return { x, y }; }

	constexpr T &operator[](size_t i) noexcept
	{
		assert(i < 3);
		return this->*MEMBERS[i];
	}

	constexpr const T &operator[](size_t i) const noexcept
	{
		assert(i < 3);
		return this->*MEMBERS[i];
	}

	constexpr bool operator==(const vector &) const noexcept = default;

private:
	constexpr static T vector::*MEMBERS[3] = { &vector::x, &vector::y, &vector::z };
};

template<CComponent T>
struct vector<T, 4> {
	T x = T(0);
	T y = T(0);
	T z = T(0);
	T w = T(0);

	constexpr vector() noexcept = default;
	constexpr explicit vector(T s) noexcept : x(s), y(s), z(s), w(s) {}
	constexpr vector(T x, T y, T z, T w) noexcept : x(x), y(y), z(z), w(w) {}
	constexpr vector(const vector<T, 2> &xy, T z, T w) noexcept : x(xy.x), y(xy.y), z(z), w(w) {}
	constexpr vector(const vector<T, 3> &xyz, T w) noexcept : x(xyz.x), y(xyz.y), z(xyz.z), w(w) {}

	template<CComponent U>
	constexpr explicit vector(const vector<U, 4> &v) noexcept : x(T(v.x)), y(T(v.y)), z(T(v.z)), w(T(v.w))
	{}

	constexpr vector<T, 2> xy() const noexcept { return { x, y }; }
	constexpr vector<T, 3> xyz() const noexcept { return { x, y, z }; }

	constexpr T &operator[](size_t i) noexcept
	{
		assert(i < 4);
		return this->*MEMBERS[i];
	}

	constexpr const T &operator[](size_t i) const noexcept
	{
		assert(i < 4);
		return this->*MEMBERS[i];
	}

	constexpr bool operator==(const vector &) const noexcept = default;

private:
	constexpr static T vector::*MEMBERS[4] = { &vector::x, &vector::y, &vector::z, &vector::w };
};

namespace detail
{

template<typename T>
inline constexpr bool IS_VECTOR = false;
template<typename T, size_t N>
inline constexpr bool IS_VECTOR<vector<T, N>> = true;

// Builds a vector of `N` components from `g(i)`, `i` goes from 0 to `N - 1`
template<size_t N, typename G>
constexpr auto make(G g) noexcept
{
	using T = decltype(g(size_t(0)));

	if constexpr (N == 2) {
		return vector<T, 2>(g(0), g(1));
	} else if constexpr (N == 3) {
		return vector<T, 3>(g(0), g(1), g(2));
	} else {
		static_assert(N == 4);
		return vector<T, 4>(g(0), g(1), g(2), g(3));
	}
}

} // namespace detail

template<typename T>
concept CVector = detail::IS_VECTOR<T>;

// --- Operators ---
// Vector-vector operators are componentwise, scalar operands are broadcast.
// Result components are cast back to `T` as integer promotion would widen them.

#define M_DETAIL_BINARY_OPERATOR(op, Concept) \
	template<Concept T, size_t N> \
	constexpr vector<T, N> operator op(const vector<T, N> &a, const vector<T, N> &b) noexcept \
	{ \
		return detail::make<N>([&](size_t i) { return T(a[i] op b[i]); }); \
	} \
	template<Concept T, size_t N> \
	constexpr vector<T, N> operator op(const vector<T, N> &a, std::type_identity_t<T> b) noexcept \
	{ \
		return detail::make<N>([&](size_t i) { return T(a[i] op b); }); \
	} \
	template<Concept T, size_t N> \
	constexpr vector<T, N> operator op(std::type_identity_t<T> a, const vector<T, N> &b) noexcept \
	{ \
		return detail::make<N>([&](size_t i) { return T(a op b[i]); }); \
	} \
	template<Concept T, size_t N> \
	constexpr vector<T, N> &operator op## = (vector<T, N> & a, const vector<T, N> &b) noexcept \
	{ \
		return a = a op b; \
	} \
	template<Concept T, size_t N> \
	constexpr vector<T, N> &operator op## = (vector<T, N> & a, std::type_identity_t<T> b) noexcept \
	{ \
		return a = a op b; \
	}

M_DETAIL_BINARY_OPERATOR(+, CScalar)
M_DETAIL_BINARY_OPERATOR(-, CScalar)
M_DETAIL_BINARY_OPERATOR(*, CScalar)
M_DETAIL_BINARY_OPERATOR(/, CScalar)
M_DETAIL_BINARY_OPERATOR(%, CIntegral)
M_DETAIL_BINARY_OPERATOR(<<, CIntegral)
M_DETAIL_BINARY_OPERATOR(>>, CIntegral)
// Bitwise for integers, logical for `bool` vectors
M_DETAIL_BINARY_OPERATOR(&, std::integral)
M_DETAIL_BINARY_OPERATOR(|, std::integral)
M_DETAIL_BINARY_OPERATOR(^, std::integral)

#undef M_DETAIL_BINARY_OPERATOR

template<CScalar T, size_t N>
constexpr vector<T, N> operator-(const vector<T, N> &a) noexcept
{
	return detail::make<N>([&](size_t i) { return T(-a[i]); });
}

template<CIntegral T, size_t N>
constexpr vector<T, N> operator~(const vector<T, N> &a) noexcept
{
	return detail::make<N>([&](size_t i) { return T(~a[i]); });
}

template<size_t N>
constexpr vector<bool, N> operator!(const vector<bool, N> &a) noexcept
{
	return detail::make<N>([&](size_t i) { return !a[i]; });
}

// --- Comparisons ---

#define M_DETAIL_COMPARISON(name, op, Concept) \
	template<Concept T, size_t N> \
	constexpr vector<bool, N> name(const vector<T, N> &a, const vector<T, N> &b) noexcept \
	{ \
		return detail::make<N>([&](size_t i) { return a[i] op b[i]; }); \
	}

M_DETAIL_COMPARISON(equal, ==, CComponent)
M_DETAIL_COMPARISON(notEqual, !=, CComponent)
M_DETAIL_COMPARISON(lessThan, <, CScalar)
M_DETAIL_COMPARISON(lessThanEqual, <=, CScalar)
M_DETAIL_COMPARISON(greaterThan, >, CScalar)
M_DETAIL_COMPARISON(greaterThanEqual, >=, CScalar)

#undef M_DETAIL_COMPARISON

template<size_t N>
constexpr bool all(const vector<bool, N> &a) noexcept
{
	for (size_t i = 0; i < N; i++) {
		if (!a[i]) {
			return false;
		}
	}
	return true;
}

template<size_t N>
constexpr bool any(const vector<bool, N> &a) noexcept
{
	for (size_t i = 0; i < N; i++) {
		if (a[i]) {
			return true;
		}
	}
	return false;
}

// Picks `a[i]` where `cond[i]` is true, `b[i]` otherwise
template<CComponent T, size_t N>
constexpr vector<T, N> select(const vector<bool, N> &cond, const vector<T, N> &a, const vector<T, N> &b) noexcept
{
	return detail::make<N>([&](size_t i) { return cond[i] ? a[i] : b[i]; });
}

// --- Scalar functions ---
// These are the building blocks of vector functions below, exposed for convenience.
// Floating-point ones use standard library at runtime and own code in constant evaluation.

namespace detail
{

// Floating-point values at or above this magnitude have no fractional part
template<CFloating T>
inline constexpr T INTEGRAL_THRESHOLD = T(uint64_t(1) << (std::numeric_limits<T>::digits - 1));

template<CFloating T>
constexpr T truncConstexpr(T x) noexcept
{
	// Also catches inf and nan, comparisons with nan are false
	if (!(x < INTEGRAL_THRESHOLD<T> && x > -INTEGRAL_THRESHOLD<T>) ) {
		return x;
	}
	return T(int64_t(x));
}

template<CFloating T>
constexpr T floorConstexpr(T x) noexcept
{
	const T t = truncConstexpr(x);
	return t > x ? t - T(1) : t;
}

template<CFloating T>
constexpr T ceilConstexpr(T x) noexcept
{
	const T t = truncConstexpr(x);
	return t < x ? t + T(1) : t;
}

template<CFloating T>
constexpr T roundConstexpr(T x) noexcept
{
	if (!(x < INTEGRAL_THRESHOLD<T> && x > -INTEGRAL_THRESHOLD<T>) ) {
		return x;
	}

	if (x < T(0)) {
		// Rounding halfway cases to even is symmetric
		return -roundConstexpr(-x);
	}

	const T f = floorConstexpr(x);
	// Exact: `f` is zero below 1, otherwise `x` and `f` are within a factor of two (Sterbenz)
	const T d = x - f;
	if (d < T(0.5)) {
		return f;
	}
	if (d > T(0.5)) {
		return f + T(1);
	}
	// Halfway, pick the even neighbour
	return int64_t(f) % 2 == 0 ? f : f + T(1);
}

template<CFloating T>
constexpr T sqrtConstexpr(T x) noexcept
{
	if (x < T(0)) {
		return std::numeric_limits<T>::quiet_NaN();
	}
	if (x == T(0) || !(x < std::numeric_limits<T>::infinity())) {
		// Zero, inf or nan
		return x;
	}

	// Scale `x` into [0.25, 4) by powers of four. The scaling is exact, works for
	// any floating-point type (subnormals included) and puts the root within a
	// factor of two of the initial guess of 1.
	T scale = T(1);
	while (x >= T(4)) {
		x /= T(4);
		scale *= T(2);
	}
	while (x < T(0.25)) {
		x *= T(4);
		scale /= T(2);
	}

	// Newton-Raphson converges quadratically, six iterations
	// bring 50% initial error below `long double` precision
	T r = T(1);
	for (int i = 0; i < 6; i++) {
		r = (r + x / r) / T(2);
	}
	return r * scale;
}

} // namespace detail

template<CScalar T>
constexpr T abs(T x) noexcept
{
	if constexpr (std::is_floating_point_v<T>) {
		if !consteval {
			return std::fabs(x);
		}
	}

	if constexpr (std::is_signed_v<T>) {
		return x < T(0) ? -x : x;
	} else {
		return x;
	}
}

// -1, 0 or 1 by the sign of `x`
template<CScalar T>
constexpr T sign(T x) noexcept
{
	if constexpr (std::is_signed_v<T>) {
		return T((x > T(0)) - (x < T(0)));
	} else {
		return T(x > T(0));
	}
}

// `min`/`max` return `a` when the comparison is false, so a NaN `a` propagates and a NaN `b` is dropped
template<CScalar T>
constexpr T min(T a, T b) noexcept
{
	return b < a ? b : a;
}

template<CScalar T>
constexpr T max(T a, T b) noexcept
{
	return a < b ? b : a;
}

// A NaN `x` propagates, `lo` and `hi` must not be NaN
template<CScalar T>
constexpr T clamp(T x, T lo, T hi) noexcept
{
	return min(max(x, lo), hi);
}

template<CFloating T>
constexpr T saturate(T x) noexcept
{
	return clamp(x, T(0), T(1));
}

template<CFloating T>
constexpr T floor(T x) noexcept
{
	if !consteval {
		return std::floor(x);
	}
	return detail::floorConstexpr(x);
}

template<CFloating T>
constexpr T ceil(T x) noexcept
{
	if !consteval {
		return std::ceil(x);
	}
	return detail::ceilConstexpr(x);
}

template<CFloating T>
constexpr T trunc(T x) noexcept
{
	if !consteval {
		return std::trunc(x);
	}
	return detail::truncConstexpr(x);
}

// Rounds to the nearest integer, halfway cases to even (banker's rounding)
template<CFloating T>
constexpr T round(T x) noexcept
{
	if !consteval {
		// Default rounding mode is round-to-nearest-even
		return std::nearbyint(x);
	}
	return detail::roundConstexpr(x);
}

// Fractional part, `x - floor(x)`
template<CFloating T>
constexpr T frac(T x) noexcept
{
	return x - floor(x);
}

template<CFloating T>
constexpr T sqrt(T x) noexcept
{
	if !consteval {
		return std::sqrt(x);
	}
	return detail::sqrtConstexpr(x);
}

template<CFloating T>
constexpr T rsqrt(T x) noexcept
{
	return T(1) / sqrt(x);
}

template<CFloating T>
constexpr bool isnan(T x) noexcept
{
	if !consteval {
		return std::isnan(x);
	}
	return x != x;
}

template<CFloating T>
constexpr bool isfinite(T x) noexcept
{
	if !consteval {
		return std::isfinite(x);
	}
	return x == x && x < std::numeric_limits<T>::infinity() && x > -std::numeric_limits<T>::infinity();
}

// `a + t * (b - a)` with a single rounding, the formula GPUs and most engines use.
// Exact at `t = 0` and monotonic in `t`, but `lerp(a, b, 1)` can miss `b` by an ulp
// when `b - a` is not representable.
//
// Constant evaluation has no fma and rounds the product separately. Its result is
// within half an ulp of `t * (b - a)` plus one ulp of the result from the runtime
// value, which is many ulps of a small result when the sum nearly cancels.
template<CFloating T>
constexpr T lerp(T a, T b, T t) noexcept
{
	if !consteval {
		return std::fma(t, b - a, a);
	}
	return t * (b - a) + a;
}

// 0 below `edge`, 1 at or above it
template<CFloating T>
constexpr T step(T edge, T x) noexcept
{
	return x < edge ? T(0) : T(1);
}

// Cubic Hermite interpolation from 0 at `edge0` to 1 at `edge1`
template<CFloating T>
constexpr T smoothstep(T edge0, T edge1, T x) noexcept
{
	const T t = saturate((x - edge0) / (edge1 - edge0));
	return t * t * (T(3) - T(2) * t);
}

template<CFloating T>
constexpr T radians(T degrees) noexcept
{
	return degrees * (std::numbers::pi_v<T> / T(180));
}

template<CFloating T>
constexpr T degrees(T radians) noexcept
{
	return radians * (T(180) / std::numbers::pi_v<T>);
}

template<CFloating T>
T sin(T x) noexcept
{
	return std::sin(x);
}

template<CFloating T>
T cos(T x) noexcept
{
	return std::cos(x);
}

template<CFloating T>
T tan(T x) noexcept
{
	return std::tan(x);
}

// Integer division rounding towards negative infinity, `b` must not be zero
template<CIntegral T>
constexpr T floorDiv(T a, T b) noexcept
{
	assert(b != T(0));
	const T q = T(a / b);
	if constexpr (std::is_signed_v<T>) {
		const T r = T(a % b);
		return (r != T(0) && ((r < T(0)) != (b < T(0)))) ? T(q - T(1)) : q;
	} else {
		return q;
	}
}

// Remainder of `floorDiv`, has the sign of `b` (unlike `%`), `b` must not be zero
template<CIntegral T>
constexpr T floorMod(T a, T b) noexcept
{
	assert(b != T(0));
	const T r = T(a % b);
	if constexpr (std::is_signed_v<T>) {
		return (r != T(0) && ((r < T(0)) != (b < T(0)))) ? T(r + b) : r;
	} else {
		return r;
	}
}

// `x - b * floor(x / b)`, the GLSL `mod`
template<CFloating T>
constexpr T floorMod(T x, T b) noexcept
{
	return x - b * floor(x / b);
}

// --- Componentwise vector functions ---

#define M_DETAIL_UNARY_FUNCTION(name, Concept) \
	template<Concept T, size_t N> \
	constexpr vector<T, N> name(const vector<T, N> &a) noexcept \
	{ \
		return detail::make<N>([&](size_t i) { return name(a[i]); }); \
	}

#define M_DETAIL_UNARY_FUNCTION_RUNTIME(name, Concept) \
	template<Concept T, size_t N> \
	vector<T, N> name(const vector<T, N> &a) noexcept \
	{ \
		return detail::make<N>([&](size_t i) { return name(a[i]); }); \
	}

#define M_DETAIL_BINARY_FUNCTION(name, Concept) \
	template<Concept T, size_t N> \
	constexpr vector<T, N> name(const vector<T, N> &a, const vector<T, N> &b) noexcept \
	{ \
		return detail::make<N>([&](size_t i) { return name(a[i], b[i]); }); \
	} \
	template<Concept T, size_t N> \
	constexpr vector<T, N> name(const vector<T, N> &a, std::type_identity_t<T> b) noexcept \
	{ \
		return detail::make<N>([&](size_t i) { return name(a[i], b); }); \
	}

#define M_DETAIL_TERNARY_FUNCTION(name, Concept) \
	template<Concept T, size_t N> \
	constexpr vector<T, N> name(const vector<T, N> &a, const vector<T, N> &b, const vector<T, N> &c) noexcept \
	{ \
		return detail::make<N>([&](size_t i) { return name(a[i], b[i], c[i]); }); \
	}

M_DETAIL_UNARY_FUNCTION(abs, CScalar)
M_DETAIL_UNARY_FUNCTION(sign, CScalar)
M_DETAIL_UNARY_FUNCTION(saturate, CFloating)
M_DETAIL_UNARY_FUNCTION(floor, CFloating)
M_DETAIL_UNARY_FUNCTION(ceil, CFloating)
M_DETAIL_UNARY_FUNCTION(trunc, CFloating)
M_DETAIL_UNARY_FUNCTION(round, CFloating)
M_DETAIL_UNARY_FUNCTION(frac, CFloating)
M_DETAIL_UNARY_FUNCTION(sqrt, CFloating)
M_DETAIL_UNARY_FUNCTION(rsqrt, CFloating)
M_DETAIL_UNARY_FUNCTION(radians, CFloating)
M_DETAIL_UNARY_FUNCTION(degrees, CFloating)
M_DETAIL_UNARY_FUNCTION_RUNTIME(sin, CFloating)
M_DETAIL_UNARY_FUNCTION_RUNTIME(cos, CFloating)
M_DETAIL_UNARY_FUNCTION_RUNTIME(tan, CFloating)

M_DETAIL_BINARY_FUNCTION(min, CScalar)
M_DETAIL_BINARY_FUNCTION(max, CScalar)
M_DETAIL_BINARY_FUNCTION(floorDiv, CIntegral)
M_DETAIL_BINARY_FUNCTION(floorMod, CScalar)

M_DETAIL_TERNARY_FUNCTION(clamp, CScalar)
M_DETAIL_TERNARY_FUNCTION(lerp, CFloating)
M_DETAIL_TERNARY_FUNCTION(smoothstep, CFloating)

#undef M_DETAIL_UNARY_FUNCTION
#undef M_DETAIL_UNARY_FUNCTION_RUNTIME
#undef M_DETAIL_BINARY_FUNCTION
#undef M_DETAIL_TERNARY_FUNCTION

template<CScalar T, size_t N>
constexpr vector<T, N> clamp(const vector<T, N> &x, std::type_identity_t<T> lo, std::type_identity_t<T> hi) noexcept
{
	return detail::make<N>([&](size_t i) { return clamp(x[i], lo, hi); });
}

template<CFloating T, size_t N>
constexpr vector<T, N> lerp(const vector<T, N> &a, const vector<T, N> &b, std::type_identity_t<T> t) noexcept
{
	return detail::make<N>([&](size_t i) { return lerp(a[i], b[i], t); });
}

template<CFloating T, size_t N>
constexpr vector<T, N> step(const vector<T, N> &edge, const vector<T, N> &x) noexcept
{
	return detail::make<N>([&](size_t i) { return step(edge[i], x[i]); });
}

template<CFloating T, size_t N>
constexpr vector<bool, N> isnan(const vector<T, N> &a) noexcept
{
	return detail::make<N>([&](size_t i) { return isnan(a[i]); });
}

template<CFloating T, size_t N>
constexpr vector<bool, N> isfinite(const vector<T, N> &a) noexcept
{
	return detail::make<N>([&](size_t i) { return isfinite(a[i]); });
}

// --- Geometric functions ---

template<CScalar T, size_t N>
constexpr T dot(const vector<T, N> &a, const vector<T, N> &b) noexcept
{
	T result = T(a[0] * b[0]);
	for (size_t i = 1; i < N; i++) {
		result = T(result + a[i] * b[i]);
	}
	return result;
}

template<CScalar T>
constexpr vector<T, 3> cross(const vector<T, 3> &a, const vector<T, 3> &b) noexcept
{
	return {
		T(a.y * b.z - a.z * b.y),
		T(a.z * b.x - a.x * b.z),
		T(a.x * b.y - a.y * b.x),
	};
}

template<CScalar T, size_t N>
constexpr T lengthSquared(const vector<T, N> &a) noexcept
{
	return dot(a, a);
}

template<CFloating T, size_t N>
constexpr T length(const vector<T, N> &a) noexcept
{
	return sqrt(dot(a, a));
}

template<CScalar T, size_t N>
constexpr T distanceSquared(const vector<T, N> &a, const vector<T, N> &b) noexcept
{
	return lengthSquared(a - b);
}

template<CFloating T, size_t N>
constexpr T distance(const vector<T, N> &a, const vector<T, N> &b) noexcept
{
	return length(a - b);
}

template<CFloating T, size_t N>
constexpr vector<T, N> normalize(const vector<T, N> &a) noexcept
{
	return a * (T(1) / length(a));
}

} // namespace m
