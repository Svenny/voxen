module;

#include <bit>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>
#include <vector>

export module tk.rng;

// Random number generation: engines, seeding utilities and distributions.
//
// Everything here is fully specified and gives the same results on every platform
// and standard library, unlike `std::*_distribution`. Floating-point distributions
// use only correctly rounded operations (`+-*/` and square root) with contraction
// to FMA disabled, so results are bit-exact across compilers and optimization levels.
// They also force precise FP semantics, to stay exact when the includer enables
// fast-math optimizations (reassociation and the like). The only exception is
// `-ffp-contract=fast` (implied by `-ffast-math`) which ignores all pragmas,
// use `-ffp-contract=fast-honor-pragmas` instead.

export namespace tk::rng
{

// Any engine producing full-range 64-bit outputs
template<typename T>
concept CRandomEngine64 = requires(T &rng) {
	// Must not throw: `std::random_device` won't qualify
	{ rng() } noexcept -> std::same_as<uint64_t>;
};

// SplitMix64 generator. Too weak for general use but good at producing
// well-mixed values even from sequential or low-entropy seeds.
class SplitMix64 {
public:
	using result_type = uint64_t;

	constexpr explicit SplitMix64(uint64_t seed) noexcept : m_state(seed) {}

	constexpr uint64_t operator()() noexcept
	{
		m_state += 0x9e3779b97f4a7c15;

		uint64_t z = m_state;
		z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
		z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
		return z ^ (z >> 31);
	}

	static constexpr uint64_t min() noexcept { return 0; }
	static constexpr uint64_t max() noexcept { return UINT64_MAX; }

	constexpr bool operator==(const SplitMix64 &) const noexcept = default;

private:
	uint64_t m_state;
};

// PCG XSL RR 128/64 generator, the default 64-bit PCG engine. Produces
// the same sequences as `pcg64_oneseq` and `pcg64` from the reference
// pcg-cpp implementation by Melissa O'Neill.
class Pcg64 {
public:
	using result_type = uint64_t;

	// Equivalent to `pcg64_oneseq(seed)`
	constexpr explicit Pcg64(uint64_t seed) noexcept : m_inc(DEFAULT_INCREMENT) { m_state = bump(seed + m_inc); }

	// Equivalent to `pcg64(seed, stream)`. Different streams give independent
	// sequences, even for the same seed.
	constexpr Pcg64(uint64_t seed, uint64_t stream) noexcept : m_inc((uint128_t(stream) << 1) | 1)
	{
		m_state = bump(seed + m_inc);
	}

	constexpr uint64_t operator()() noexcept
	{
		m_state = bump(m_state);
		return output(m_state);
	}

	// Skip `delta` outputs in O(log(delta)) time
	constexpr void advance(uint64_t delta) noexcept
	{
		// Brown, "Random Number Generation with Arbitrary Stride" (1994)
		uint128_t cur_mult = MULTIPLIER;
		uint128_t cur_plus = m_inc;
		uint128_t acc_mult = 1;
		uint128_t acc_plus = 0;

		while (delta > 0) {
			if (delta & 1) {
				acc_mult *= cur_mult;
				acc_plus = acc_plus * cur_mult + cur_plus;
			}

			cur_plus = (cur_mult + 1) * cur_plus;
			cur_mult *= cur_mult;
			delta >>= 1;
		}

		m_state = acc_mult * m_state + acc_plus;
	}

	static constexpr uint64_t min() noexcept { return 0; }
	static constexpr uint64_t max() noexcept { return UINT64_MAX; }

	constexpr bool operator==(const Pcg64 &) const noexcept = default;

private:
	__extension__ typedef unsigned __int128 uint128_t;

	constexpr static uint128_t MULTIPLIER = (uint128_t(2549297995355413924) << 64) | 4865540595714422341;
	constexpr static uint128_t DEFAULT_INCREMENT = (uint128_t(6364136223846793005) << 64) | 1442695040888963407;

	uint128_t m_state;
	uint128_t m_inc;

	constexpr uint128_t bump(uint128_t state) const noexcept { return state * MULTIPLIER + m_inc; }

	constexpr static uint64_t output(uint128_t state) noexcept
	{
		const uint64_t value = uint64_t(state >> 64) ^ uint64_t(state);
		return std::rotr(value, int(state >> 122));
	}
};

// Derive a seed for an independent sub-sequence from `seed` and an
// arbitrary `key` (e.g. a subsystem ID or a world coordinate hash)
constexpr uint64_t deriveSeed(uint64_t seed, uint64_t key) noexcept
{
	return SplitMix64(SplitMix64(seed)() ^ key)();
}

// Make a non-deterministic seed from system entropy source
uint64_t entropySeed();

// Uniformly distributed integer in [min; max], both ends inclusive.
// Unbiased, uses Lemire's nearly divisionless method.
template<std::integral T, CRandomEngine64 R>
constexpr T uniformInt(R &rng, T min, T max) noexcept
{
	assert(min <= max);
	static_assert(sizeof(T) <= sizeof(uint64_t));

	using U = std::make_unsigned_t<T>;
	// Number of values in the range, zero means the full 64-bit range.
	// Cast back to `U`, subtraction promotes types smaller than `int` to it.
	const uint64_t range = uint64_t(U(U(max) - U(min))) + 1;

	if (range == 0) {
		return T(rng());
	}

	__extension__ typedef unsigned __int128 uint128_t;

	uint128_t m = uint128_t(rng()) * range;
	if (uint64_t(m) < range) {
		const uint64_t threshold = (0 - range) % range;
		while (uint64_t(m) < threshold) {
			m = uint128_t(rng()) * range;
		}
	}

	return T(U(min) + U(m >> 64));
}

// Uniformly distributed real number in [0; 1)
template<std::floating_point T, CRandomEngine64 R>
constexpr T uniform01(R &rng) noexcept
{
	static_assert(std::numeric_limits<T>::radix == 2);
	constexpr int DIGITS = std::numeric_limits<T>::digits;
	static_assert(DIGITS < 64); // 64 will overflow 1 << DIGITS

	// Take the topmost bits, they are the best in some generators
	return T(rng() >> (64 - DIGITS)) * (T(1) / T(uint64_t(1) << DIGITS));
}

// Uniformly distributed real number in [min; max)
template<std::floating_point T, CRandomEngine64 R>
constexpr T uniformReal(R &rng, T min, T max) noexcept
{
#pragma float_control(precise, on)
#pragma clang fp contract(off)
	assert(min < max);
	// Ranges wider than the largest finite `T` (like [-FLT_MAX; FLT_MAX]) are not supported,
	// handling them would slow down the common case for no practical use
	assert(std::isfinite(max - min));

	const T value = min + (max - min) * uniform01<T>(rng);
	// Rounding can produce exactly `max`, keep the range half-open
	return value < max ? value : std::nextafter(max, min);
}

// Returns `true` with probability `p`
template<std::floating_point T, CRandomEngine64 R>
constexpr bool bernoulli(R &rng, T p) noexcept
{
	return uniform01<T>(rng) < p;
}

// Piecewise linear distribution, same parameters as `std::piecewise_linear_distribution`.
// Probability density is defined at points `points[i]` as (relative) `weights[i]`,
// linearly interpolated between neighbor points and zero outside of the full range.
template<std::floating_point T>
class PiecewiseLinearDistribution {
public:
	// `points` must be strictly increasing, contain at least two values, and
	// `weights` must be of the same size, non-negative with a non-zero total area
	PiecewiseLinearDistribution(std::span<const T> points, std::span<const T> weights)
		: m_points(points.begin(), points.end()), m_weights(weights.begin(), weights.end())
	{
#pragma float_control(precise, on)
#pragma clang fp contract(off)
		assert(points.size() >= 2);
		assert(points.size() == weights.size());

		m_cumulative_areas.reserve(points.size() - 1);

		T total_area = 0;
		for (size_t i = 0; i < points.size() - 1; i++) {
			assert(points[i] < points[i + 1]);
			assert(weights[i] >= 0);

			total_area += T(0.5) * (weights[i] + weights[i + 1]) * (points[i + 1] - points[i]);
			m_cumulative_areas.push_back(total_area);
		}

		assert(total_area > 0);
	}

	template<CRandomEngine64 R>
	T operator()(R &rng) const noexcept
	{
#pragma float_control(precise, on)
#pragma clang fp contract(off)
		const T target_area = uniform01<T>(rng) * m_cumulative_areas.back();

		// Find the segment containing the target area (linear search
		// should be fine, there are usually just a few segments)
		size_t segment = 0;
		while (segment < m_cumulative_areas.size() - 1 && m_cumulative_areas[segment] <= target_area) {
			segment++;
		}

		const T base_area = segment > 0 ? m_cumulative_areas[segment - 1] : T(0);
		const T area = target_area - base_area;

		const T x0 = m_points[segment];
		const T length = m_points[segment + 1] - x0;
		const T w0 = m_weights[segment];
		const T slope = (m_weights[segment + 1] - w0) / length;

		if (area <= 0) {
			return x0;
		}

		// Solve `w0 * x + slope * x^2 / 2 = area` for `x` in [0; length].
		// This form of the quadratic root avoids cancellation and handles zero slope.
		const T x = T(2) * area / (w0 + std::sqrt(w0 * w0 + T(2) * slope * area));
		return x < length ? x0 + x : std::nextafter(x0 + length, x0);
	}

private:
	std::vector<T> m_points;
	std::vector<T> m_weights;
	std::vector<T> m_cumulative_areas;
};

} // namespace tk::rng
