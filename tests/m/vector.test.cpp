#include <m/format.hpp>
#include <m/m.hpp>

#include "../test_common.hpp"

#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <fmt/format.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <type_traits>

namespace m
{

namespace
{

constexpr float NAN_F = std::numeric_limits<float>::quiet_NaN();
constexpr double INF_D = std::numeric_limits<double>::infinity();
constexpr double NAN_D = std::numeric_limits<double>::quiet_NaN();

// Layout guarantees, structs shared with GPU rely on them
static_assert(sizeof(float3) == 12 && alignof(float3) == 4);
static_assert(sizeof(double2) == 16 && alignof(double2) == 8);
static_assert(sizeof(uint8_t3) == 3 && alignof(uint8_t3) == 1);
static_assert(sizeof(int16_t4) == 8 && alignof(int16_t4) == 2);
static_assert(std::is_trivially_copyable_v<float3> && std::is_standard_layout_v<float3>);

// Conversions between component types and scalar broadcast are explicit, there is no shrinking
static_assert(std::is_constructible_v<int3, float3> && !std::is_convertible_v<float3, int3>);
static_assert(std::is_constructible_v<float3, double3> && !std::is_convertible_v<double3, float3>);
static_assert(std::is_constructible_v<float3, float> && !std::is_convertible_v<float, float3>);
static_assert(!std::is_constructible_v<float3, float4>);
// Arithmetic on bool vectors and bit operations on float vectors don't exist
template<typename A, typename B = A>
concept CAddable = requires(A a, B b) { a + b; };
template<typename A>
concept CBitAndable = requires(A a) { a & a; };
template<typename A>
concept CModable = requires(A a) { a % a; };
static_assert(!CAddable<bool2> && !CBitAndable<float2> && !CModable<float2>);
static_assert(CAddable<int2> && CBitAndable<int2> && CModable<int2> && CBitAndable<bool2>);
// Vectors of different types don't mix
static_assert(!CAddable<float3, int3> && !CAddable<float3, float2>);

// Construction and component access
static_assert(float3() == float3(0, 0, 0));
static_assert(float3(2) == float3(2, 2, 2));
static_assert(float3(float2(1, 2), 3) == float3(1, 2, 3));
static_assert(float4(float3(1, 2, 3), 4) == float4(1, 2, 3, 4));
static_assert(float4(float2(1, 2), 3, 4) == float4(1, 2, 3, 4));
static_assert(float4(1, 2, 3, 4).xyz() == float3(1, 2, 3));
static_assert(float4(1, 2, 3, 4).xy() == float2(1, 2));
static_assert(float3(1, 2, 3).xy() == float2(1, 2));
static_assert(float3(1, 2, 3)[2] == 3);
// Conversion to integers truncates, like `static_cast`
static_assert(int3(float3(1.9f, -1.9f, 0.5f)) == int3(1, -1, 0));
static_assert(bool3(int3(0, 1, 2)) == bool3(false, true, true));
static_assert(float3(bool3(true, false, true)) == float3(1, 0, 1));

constexpr float3 indexAssign()
{
	float3 v;
	v[0] = 1;
	v[1] = 2;
	v[2] = 3;
	return v;
}
static_assert(indexAssign() == float3(1, 2, 3));

// Arithmetic
static_assert(float3(1, 2, 3) + float3(4, 5, 6) == float3(5, 7, 9));
static_assert(float3(1, 2, 3) - float3(4, 5, 6) == float3(-3, -3, -3));
static_assert(float3(1, 2, 3) * float3(4, 5, 6) == float3(4, 10, 18));
static_assert(float3(1, 2, 3) * 2 == float3(2, 4, 6)); // Scalar converts to the component type
static_assert(2 * float3(1, 2, 3) == float3(2, 4, 6));
static_assert(float3(1, 2, 3) / 2 == float3(0.5f, 1, 1.5f));
static_assert(1.0f / float2(2, 4) == float2(0.5f, 0.25f));
static_assert(-int3(1, -2, 3) == int3(-1, 2, -3));
static_assert(int3(7, -7, 8) / 2 == int3(3, -3, 4));
static_assert(int3(7, -7, 8) % 3 == int3(1, -1, 2));
static_assert((int3(6, 5, 1) & 3) == int3(2, 1, 1));
static_assert((int3(6, 5, 1) | int3(1, 2, 2)) == int3(7, 7, 3));
static_assert((int3(6, 5, 1) ^ int3(1, 2, 2)) == int3(7, 7, 3));
static_assert((uint3(1, 2, 3) << 2) == uint3(4, 8, 12));
static_assert((uint3(4, 8, 12) >> uint3(1, 2, 3)) == uint3(2, 2, 1));
static_assert(~uint8_t2(0, 0xF0) == uint8_t2(0xFF, 0x0F));
// Narrow types wrap around instead of staying promoted
static_assert(uint8_t3(200, 100, 0) + uint8_t3(100, 100, 0) == uint8_t3(44, 200, 0));
static_assert(uint8_t2(2, 3) * uint8_t2(200, 100) == uint8_t2(144, 44));
static_assert((uint8_t2(1, 2) << 7) == uint8_t2(128, 0));

constexpr float3 compound()
{
	float3 v(1, 2, 3);
	v += float3(1);
	v *= 2;
	v -= 1;
	v /= float3(1, 5, 7);
	return v;
}
static_assert(compound() == float3(3, 1, 1));

// Bool vectors
static_assert((bool2(true, false) | bool2(false, false)) == bool2(true, false));
static_assert((bool2(true, false) & bool2(true, true)) == bool2(true, false));
static_assert((bool2(true, false) ^ bool2(true, true)) == bool2(false, true));
static_assert(!bool3(true, false, true) == bool3(false, true, false));

constexpr bool2 boolCompound()
{
	bool2 v;
	v |= bool2(true, false);
	v &= bool2(true, true);
	return v;
}
static_assert(boolCompound() == bool2(true, false));

// Comparisons
static_assert(lessThan(int3(1, 2, 3), int3(2, 2, 2)) == bool3(true, false, false));
static_assert(lessThanEqual(int3(1, 2, 3), int3(2, 2, 2)) == bool3(true, true, false));
static_assert(greaterThan(int3(1, 2, 3), int3(2, 2, 2)) == bool3(false, false, true));
static_assert(greaterThanEqual(int3(1, 2, 3), int3(2, 2, 2)) == bool3(false, true, true));
static_assert(equal(float2(1, 2), float2(1, 3)) == bool2(true, false));
static_assert(notEqual(bool2(true, false), bool2(true, true)) == bool2(false, true));
static_assert(all(bool3(true, true, true)) && !all(bool3(true, false, true)));
static_assert(any(bool3(false, false, true)) && !any(bool3(false, false, false)));
static_assert(select(bool3(true, false, true), int3(1, 2, 3), int3(4, 5, 6)) == int3(1, 5, 3));

// Componentwise functions
static_assert(abs(int3(-1, 2, -3)) == int3(1, 2, 3));
static_assert(abs(uint2(1, 2)) == uint2(1, 2));
static_assert(abs(float2(-1.5f, 1.5f)) == float2(1.5f, 1.5f));
static_assert(sign(float3(-2.5f, 0, 7)) == float3(-1, 0, 1));
static_assert(sign(uint2(0, 3)) == uint2(0, 1));
static_assert(min(int3(1, 5, 3), int3(2, 2, 2)) == int3(1, 2, 2));
static_assert(max(int3(1, 5, 3), 2) == int3(2, 5, 3));
static_assert(clamp(float3(-1, 0.5f, 2), 0.0f, 1.0f) == float3(0, 0.5f, 1));
static_assert(clamp(int3(-1, 5, 20), int3(0, 0, 0), int3(10, 3, 10)) == int3(0, 3, 10));
static_assert(saturate(float2(-3, 0.25f)) == float2(0, 0.25f));
static_assert(floor(float3(1.5f, -1.5f, 2)) == float3(1, -2, 2));
static_assert(ceil(float3(1.5f, -1.5f, 2)) == float3(2, -1, 2));
static_assert(trunc(float3(1.5f, -1.5f, 2)) == float3(1, -1, 2));
// Halfway cases round to even
static_assert(round(double4(0.5, 1.5, 2.5, -0.5)) == double4(0, 2, 2, 0));
static_assert(round(float3(0.4999f, 2.51f, -2.51f)) == float3(0, 3, -3));
static_assert(frac(float2(1.25f, -1.25f)) == float2(0.25f, 0.75f));
static_assert(lerp(float3(0, 10, -2), float3(10, 20, 2), 0.25f) == float3(2.5f, 12.5f, -1));
static_assert(lerp(float2(1, 2), float2(3, 4), float2(0, 1)) == float2(1, 4));
static_assert(lerp(0.0, 1.0, 1.0) == 1.0 && lerp(-1.0, 1.0, 0.5) == 0.0);
static_assert(lerp(0.1f, 0.7f, 0.0f) == 0.1f && lerp(double2(0.3, -5), double2(1e9, 7), 0.0) == double2(0.3, -5));
static_assert(step(float2(1, 1), float2(0.5f, 1)) == float2(0, 1));
static_assert(smoothstep(0.0f, 1.0f, 0.5f) == 0.5f && smoothstep(0.0, 2.0, -1.0) == 0.0);
static_assert(smoothstep(float2(0, 0), float2(1, 1), float2(1, 2)) == float2(1, 1));
static_assert(radians(180.0) == std::numbers::pi);
static_assert(degrees(float2(std::numbers::pi_v<float>, 0)) == float2(180, 0));
static_assert(isnan(float2(NAN_F, 1)) == bool2(true, false));
static_assert(isfinite(double3(1, INF_D, NAN_D)) == bool3(true, false, false));
static_assert(floorDiv(int2(7, -7), 2) == int2(3, -4) && floorMod(int2(7, -7), 2) == int2(1, 1));
// NaN is never checked for, a NaN `x` propagates through clamping (HLSL `saturate` would give 0)
static_assert(isnan(min(NAN_F, 1.0f)) && isnan(max(NAN_F, 1.0f)) && min(1.0f, NAN_F) == 1.0f);
static_assert(isnan(clamp(NAN_D, 0.0, 1.0)) && isnan(saturate(NAN_F)));
static_assert(isnan(saturate(float2(NAN_F, 2)).x) && saturate(float2(NAN_F, 2)).y == 1);
static_assert(isnan(abs(NAN_F)) && isnan(floor(NAN_D)) && isnan(round(NAN_F)));
static_assert(floorDiv(-6, 3) == -2 && floorMod(-6, 3) == 0);
static_assert(floorDiv(7, -3) == -3 && floorMod(7, -3) == -2);
static_assert(floorDiv(7u, 2u) == 3u && floorMod(7u, 2u) == 1u);
static_assert(floorMod(float2(5.5f, -0.5f), 2.0f) == float2(1.5f, 1.5f));

// Geometric functions
static_assert(dot(int3(1, 2, 3), int3(4, 5, 6)) == 32);
static_assert(dot(float2(0.5f, 2), float2(2, 0.25f)) == 1.5f);
// Right-handed cross product
static_assert(cross(float3(1, 0, 0), float3(0, 1, 0)) == float3(0, 0, 1));
static_assert(cross(float3(0, 1, 0), float3(0, 0, 1)) == float3(1, 0, 0));
static_assert(cross(float3(0, 0, 1), float3(1, 0, 0)) == float3(0, 1, 0));
static_assert(lengthSquared(int2(3, 4)) == 25);
static_assert(distanceSquared(int2(1, 1), int2(4, 5)) == 25);
static_assert(abs(length(float2(3, 4)) - 5) < 1e-6f);
static_assert(abs(distance(double3(1, 2, 3), double3(1, 2, 5)) - 2) < 1e-15);
static_assert(abs(normalize(float3(0, 3, 0)).y - 1) < 1e-6f);

// Compile-time floating-point functions must agree with their runtime versions.
// `sqrt` is allowed one ulp of difference, `lerp` has its own bound (see below),
// the others must be exact.
constexpr auto ROUNDING_INPUTS = std::to_array<double>({ 0.0, 0.25, 0.5, 0.75, 1.0, 1.5, 2.5, 3.5, -0.25, -0.5, -0.75,
	-1.5, -2.5, 123456.5, -123456.5, 4503599627370496.5, 4503599627370495.5, -4503599627370495.5, 9007199254740993.0,
	1e300, -1e300, INF_D, -INF_D, 0.49999999999999994 });

struct RoundingResults {
	std::array<double, ROUNDING_INPUTS.size()> floor, ceil, trunc, round;
};

constexpr RoundingResults computeRounding()
{
	RoundingResults r;
	for (size_t i = 0; i < ROUNDING_INPUTS.size(); i++) {
		r.floor[i] = floor(ROUNDING_INPUTS[i]);
		r.ceil[i] = ceil(ROUNDING_INPUTS[i]);
		r.trunc[i] = trunc(ROUNDING_INPUTS[i]);
		r.round[i] = round(ROUNDING_INPUTS[i]);
	}
	return r;
}

constexpr auto SQRT_INPUTS = std::to_array<double>({ 0.0, 1.0, 2.0, 4.0, 25.0, 0.5, 1e-300, 1e300, 123.456,
	3.141592653589793, 1e-10, 65536.0, 7.0, 99.99, 1.7976931348623157e308, 2.2250738585072014e-308, 1e-310, 5e-324 });

constexpr auto computeSqrt()
{
	std::array<double, SQRT_INPUTS.size()> r;
	for (size_t i = 0; i < SQRT_INPUTS.size(); i++) {
		r[i] = sqrt(SQRT_INPUTS[i]);
	}
	return r;
}

template<CFloating T>
bool withinOneUlp(T a, T b)
{
	return a >= std::nextafter(b, -std::numeric_limits<T>::infinity())
		&& a <= std::nextafter(b, std::numeric_limits<T>::infinity());
}

template<CFloating T>
T ulpOf(T x)
{
	x = abs(x);
	return std::nextafter(x, std::numeric_limits<T>::infinity()) - x;
}

// Pseudo-random `lerp` inputs spanning magnitudes, a third of them nearly cancelling.
// Generated at compile time so that `lerp` can be constant-evaluated on them.
struct LerpInput {
	double a, b, t;
};

constexpr auto LERP_INPUTS = [] {
	std::array<LerpInput, 600> inputs {};
	constexpr double SCALES[] = { 1e-6, 1e-3, 1.0, 1e3, 1e6 };

	uint64_t state = 0x9E3779B97F4A7C15;
	auto next = [&] {
		// LCG, top bits are the good ones
		state = state * 6364136223846793005 + 1442695040888963407;
		return double(state >> 11) * 0x1p-53; // [0, 1)
	};
	auto signed_scaled = [&] { return (next() * 20 - 10) * SCALES[state % 5]; };

	for (size_t i = 0; i < inputs.size(); i++) {
		LerpInput &in = inputs[i];
		in.a = signed_scaled();
		in.b = signed_scaled();
		in.t = next();
		if (i % 3 == 1) {
			// `a + t * (b - a)` is nearly zero
			in.t = 0.25 + in.t / 2;
			in.b = -in.a * (1 - in.t) / in.t;
		}
	}
	return inputs;
}();

constexpr auto computeLerp()
{
	std::array<double, LERP_INPUTS.size()> r {};
	for (size_t i = 0; i < LERP_INPUTS.size(); i++) {
		r[i] = lerp(LERP_INPUTS[i].a, LERP_INPUTS[i].b, LERP_INPUTS[i].t);
	}
	return r;
}

} // namespace

TEST_CASE("Constant evaluation of rounding functions matches runtime", "[m::vector]")
{
	constexpr RoundingResults COMPILE_TIME = computeRounding();
	const RoundingResults run_time = computeRounding();

	CHECK(COMPILE_TIME.floor == run_time.floor);
	CHECK(COMPILE_TIME.ceil == run_time.ceil);
	CHECK(COMPILE_TIME.trunc == run_time.trunc);
	CHECK(COMPILE_TIME.round == run_time.round);
}

TEST_CASE("Constant evaluation of sqrt is within one ulp of runtime", "[m::vector]")
{
	constexpr auto COMPILE_TIME = computeSqrt();
	const auto run_time = computeSqrt();

	size_t mismatches = 0;
	for (size_t i = 0; i < SQRT_INPUTS.size(); i++) {
		if (!withinOneUlp(COMPILE_TIME[i], run_time[i])) {
			mismatches++;
			UNSCOPED_INFO(
				fmt::format("sqrt({}) = {} at compile time, {} at runtime", SQRT_INPUTS[i], COMPILE_TIME[i], run_time[i]));
		}
	}
	CHECK(mismatches == 0);

	// Types other than `double` take the same code path but with different exponent ranges
	constexpr float SUBNORMAL_F = sqrt(1e-40f);
	constexpr float LARGE_F = sqrt(3e38f);
	CHECK(withinOneUlp(SUBNORMAL_F, std::sqrt(1e-40f)));
	CHECK(withinOneUlp(LARGE_F, std::sqrt(3e38f)));

	constexpr long double LARGE_L = sqrt(1e300L);
	CHECK(withinOneUlp(LARGE_L, std::sqrt(1e300L)));
	// Prevent -Wliteral-range where long double maps to double
#if __SIZEOF_LONG_DOUBLE__ > 8
	constexpr long double TINY_L = sqrt(1e-4940L);
	CHECK(withinOneUlp(TINY_L, std::sqrt(1e-4940L)));
#endif
}

TEST_CASE("Geometric functions", "[m::vector]")
{
	using Catch::Matchers::WithinAbs;

	CHECK_THAT(length(float3(1, 2, 2)), WithinAbs(3.0, 1e-6));
	CHECK_THAT(distance(double2(1, 1), double2(4, 5)), WithinAbs(5.0, 1e-12));
	CHECK_THAT(length(normalize(double3(1, -2, 3))), WithinAbs(1.0, 1e-12));
	CHECK(normalize(float3(0, 0, -2)) == float3(0, 0, -1));
	CHECK_THAT(dot(normalize(float3(1, 1, 0)), normalize(float3(-1, 1, 0))), WithinAbs(0.0, 1e-6));
}

TEST_CASE("Constant evaluation of lerp is within the rounding of its product", "[m::vector]")
{
	constexpr auto COMPILE_TIME = computeLerp();

	size_t violations = 0;
	size_t differences = 0;
	for (size_t i = 0; i < LERP_INPUTS.size(); i++) {
		const auto [a, b, t] = LERP_INPUTS[i];
		const double run_time = lerp(a, b, t);
		// Runtime fuses this product into the sum, constant evaluation rounds it first
		const double bound = 0.5 * ulpOf(t * (b - a)) + ulpOf(run_time);

		differences += COMPILE_TIME[i] != run_time;
		if (abs(COMPILE_TIME[i] - run_time) > bound) {
			violations++;
			UNSCOPED_INFO(
				fmt::format("lerp({}, {}, {}) = {} at compile time, {} at runtime", a, b, t, COMPILE_TIME[i], run_time));
		}
	}
	CHECK(violations == 0);
	// The bound is not vacuous, the two really differ
	CHECK(differences > 0);
}

TEST_CASE("Runtime lerp is a single fused multiply-add", "[m::vector]")
{
	const float a = 0.1f, b = 0.7f, t = 0.3f;
	CHECK(lerp(a, b, t) == std::fma(t, b - a, a));
	CHECK(lerp(float2(a, b), float2(b, a), t) == float2(std::fma(t, b - a, a), std::fma(t, a - b, b)));
	CHECK(lerp(a, b, 0.0f) == a);
	// Monotonic in `t`
	float prev = lerp(a, b, 0.0f);
	size_t decreases = 0;
	for (int i = 1; i <= 1000; i++) {
		const float cur = lerp(a, b, float(i) / 1000.0f);
		decreases += cur < prev;
		prev = cur;
	}
	CHECK(decreases == 0);
}

TEST_CASE("Trigonometry and rsqrt", "[m::vector]")
{
	using Catch::Matchers::WithinAbs;

	const float3 angles(0, radians(90.0f), radians(180.0f));
	const float3 s = sin(angles);
	const float3 c = cos(angles);
	CHECK_THAT(s.x, WithinAbs(0.0, 1e-6));
	CHECK_THAT(s.y, WithinAbs(1.0, 1e-6));
	CHECK_THAT(c.z, WithinAbs(-1.0, 1e-6));
	CHECK_THAT(rsqrt(4.0), WithinAbs(0.5, 1e-12));
	CHECK(rsqrt(float2(4, 16)) == float2(0.5f, 0.25f));
}

TEST_CASE("Formatting", "[m::vector]")
{
	CHECK(fmt::format("{}", float3(1, 2.5f, -3)) == "(1, 2.5, -3)");
	CHECK(fmt::format("{:.2f}", double2(1, 0.125)) == "(1.00, 0.12)");
	CHECK(fmt::format("{:>3}", int4(1, 22, 333, 4)) == "(  1,  22, 333,   4)");
	CHECK(fmt::format("{}", bool2(true, false)) == "(true, false)");
	CHECK(fmt::format("{:x}", uint8_t2(255, 16)) == "(ff, 10)");
}

} // namespace m
