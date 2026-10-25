#include <tk/rng.hpp>

#include "../test_common.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace tk::rng
{

namespace
{

// Collects `N` results of `fn()` to check them with one assertion.
// Per-sample assertions in loops would inflate the assertion count.
template<size_t N, typename F>
auto generate(F &&fn)
{
	std::array<decltype(fn()), N> values;
	for (auto &value : values) {
		value = fn();
	}
	return values;
}

template<CRandomEngine64 R, size_t N>
void checkSequence(R rng, const std::array<uint64_t, N> &expected)
{
	CHECK(generate<N>(rng) == expected);
}

} // namespace

TEST_CASE("'SplitMix64' matches the reference implementation", "[tk::rng]")
{
	// Test vector from the reference implementation (Sebastiano Vigna)
	checkSequence(SplitMix64(1234567),
		std::to_array<uint64_t>({ 6457827717110365317, 3203168211198807973, 9817491932198370423u }));
	checkSequence(SplitMix64(0),
		std::to_array<uint64_t>({ 0xe220a8397b1dcdaf, 0x6e789e6aa1b965f4, 0x06c45d188009454f }));
}

TEST_CASE("'Pcg64' matches pcg-cpp 'pcg64_oneseq'", "[tk::rng]")
{
	checkSequence(Pcg64(0),
		std::to_array<uint64_t>({ 0x01070196e695f8f1, 0x703ec840c59f4493, 0xe54954914b3a44fa, 0x96130ff204b9285e,
			0x7d9fdef535ceb21a, 0x666feed42e1219a0 }));
	checkSequence(Pcg64(1),
		std::to_array<uint64_t>({ 0xe175e32ed3507bfa, 0xc0bf922a0b283109, 0x140bfa21e68785bb, 0xc5ec8bcc4fe35830,
			0x4dd8ed1ca22869c5, 0xc9bffa29c802ef4c }));
	checkSequence(Pcg64(42),
		std::to_array<uint64_t>({ 0x287472e87ff5705a, 0xbbd190b04ed0b545, 0xb6cee3580db14880, 0xbf5f7d7e4c3d1864,
			0x734eedbe7e50bbc5, 0xa5b6b5f867691c77 }));
	checkSequence(Pcg64(0xcafef00dd15ea5e5),
		std::to_array<uint64_t>({ 0xcf7dbe684e0c4045, 0x15642875dfe1e67c, 0x32f049df2f50d811, 0x98c1d0a163e1f856,
			0x0743e58f0360d766, 0xfee9e4fc03479bf6 }));
	checkSequence(Pcg64(UINT64_MAX),
		std::to_array<uint64_t>({ 0x3b17d015242767f3, 0x4180161fdb39123e, 0xd58a3e399c161fa3, 0x4d591ceb3fb24dce,
			0xc95c92c4dcfae7c7, 0xeb906ebe0dc66abd }));
}

TEST_CASE("'Pcg64' with stream matches pcg-cpp 'pcg64'", "[tk::rng]")
{
	checkSequence(Pcg64(0, 0),
		std::to_array<uint64_t>({ 0xd4feb4e5a4bcfe09, 0xe85a7fe071b026e6, 0x3a5b9037fe928c11, 0x7b044380d100f216,
			0x1c7850a6b6d83e6a, 0x240b82fcc04f0926 }));
	checkSequence(Pcg64(42, 54),
		std::to_array<uint64_t>({ 0x86b1da1d72062b68, 0x1304aa46c9853d39, 0xa3670e9e0dd50358, 0xf9090e529a7dae00,
			0xc85b9fd837996f2c, 0x606121f8e3919196 }));
	checkSequence(Pcg64(0xcafef00dd15ea5e5, 0x123456789abcdef),
		std::to_array<uint64_t>({ 0xa3bb1a184298b792, 0xa1912fc8d74a7b78, 0x15f1bb80003f0c91, 0x7f66b2d493436baf,
			0x2981445a7470b47c, 0xf3a62585c3ca9c44 }));
}

TEST_CASE("'Pcg64::advance' skips outputs", "[tk::rng]")
{
	Pcg64 rng(42);
	rng.advance(1000);
	CHECK(rng() == 0xc55e72ae9fadc27d);

	rng = Pcg64(42);
	rng.advance(UINT64_MAX);
	CHECK(rng() == 0x6fb61741018e006b);

	Pcg64 a(123, 456);
	Pcg64 b = a;
	for (int i = 0; i < 777; i++) {
		a();
	}
	b.advance(777);
	CHECK(a == b);

	b.advance(0);
	CHECK(a == b);
}

TEST_CASE("'deriveSeed' gives distinct seeds", "[tk::rng]")
{
	static_assert(deriveSeed(42, 1) == 0xf34fe9248c9342e5);
	static_assert(deriveSeed(42, 2) == 0x725395388690ae46);

	CHECK(deriveSeed(42, 1) != deriveSeed(43, 1));
	CHECK(deriveSeed(0, 0) != 0);
}

TEST_CASE("'uniformInt' produces values in range", "[tk::rng]")
{
	Pcg64 rng(1);

	SECTION("Small signed range")
	{
		std::array<int, 7> counts {};
		int out_of_range = 0;
		for (int i = 0; i < 70000; i++) {
			int value = uniformInt(rng, -3, 3);
			if (value >= -3 && value <= 3) {
				counts[size_t(value + 3)]++;
			} else {
				out_of_range++;
			}
		}

		CHECK(out_of_range == 0);
		// Roughly 10000 each, 5 sigma is about 500
		CHECK(std::ranges::min(counts) > 9500);
		CHECK(std::ranges::max(counts) < 10500);
	}

	SECTION("Single value range")
	{
		auto ints = generate<10>([&] { return uniformInt(rng, 5, 5); });
		CHECK(std::ranges::all_of(ints, [](int v) { return v == 5; }));
		auto mins = generate<10>([&] { return uniformInt<int64_t>(rng, INT64_MIN, INT64_MIN); });
		CHECK(std::ranges::all_of(mins, [](int64_t v) { return v == INT64_MIN; }));
	}

	SECTION("Full and near-full ranges")
	{
		Pcg64 copy = rng;
		CHECK(uniformInt<uint64_t>(rng, 0, UINT64_MAX) == copy());
		CHECK(uniformInt<int64_t>(rng, INT64_MIN, INT64_MAX) == int64_t(copy()));

		auto unsigned_values = generate<1000>([&] { return uniformInt<uint64_t>(rng, 1, UINT64_MAX); });
		CHECK(std::ranges::min(unsigned_values) >= 1);
		auto signed_values = generate<1000>([&] { return uniformInt<int64_t>(rng, INT64_MIN + 1, INT64_MAX); });
		CHECK(std::ranges::min(signed_values) >= INT64_MIN + 1);
	}

	SECTION("Small types")
	{
		auto u8 = generate<1000>([&] { return uniformInt<uint8_t>(rng, 250, 255); });
		CHECK(int(std::ranges::min(u8)) == 250);
		CHECK(int(std::ranges::max(u8)) == 255);
		auto i16 = generate<1000>([&] { return uniformInt<int16_t>(rng, -32768, -32760); });
		CHECK(std::ranges::min(i16) == -32768);
		CHECK(std::ranges::max(i16) == -32760);

		// Ranges crossing zero, `max - min` gets promoted to `int` here
		auto i8 = generate<1000>([&] { return uniformInt<int8_t>(rng, -1, 1); });
		CHECK(int(std::ranges::min(i8)) == -1);
		CHECK(int(std::ranges::max(i8)) == 1);
		auto i16_small = generate<1000>([&] { return uniformInt<int16_t>(rng, -2, 2); });
		CHECK(std::ranges::min(i16_small) == -2);
		CHECK(std::ranges::max(i16_small) == 2);
	}
}

TEST_CASE("'uniformReal' produces values in range", "[tk::rng]")
{
	Pcg64 rng(2);

	double sum = 0.0;
	float f_min = 1.0f;
	float f_max = -1.0f;
	double d_min = 101.0;
	double d_max = 99.0;
	for (int i = 0; i < 100000; i++) {
		float f = uniformReal(rng, -1.0f, 1.0f);
		f_min = std::min(f_min, f);
		f_max = std::max(f_max, f);

		double d = uniformReal(rng, 100.0, 100.0000001);
		d_min = std::min(d_min, d);
		d_max = std::max(d_max, d);

		sum += double(f);
	}

	CHECK(f_min >= -1.0f);
	CHECK(f_max < 1.0f);
	CHECK(d_min >= 100.0);
	CHECK(d_max < 100.0000001);

	// Mean of 100000 samples, sigma is about 0.002
	CHECK(sum / 100000.0 > -0.01);
	CHECK(sum / 100000.0 < 0.01);

	// Tiny range (just one representable value)
	const float lo = 1.0f;
	const float hi = std::nextafter(1.0f, 2.0f);
	auto tiny = generate<100>([&] { return uniformReal(rng, lo, hi); });
	CHECK(std::ranges::all_of(tiny, [&](float v) { return v == lo; }));

	// Engine returning the maximal value, ensure rounding doesn't reach `max`
	struct MaxEngine {
		uint64_t operator()() noexcept { return UINT64_MAX; }
	} max_engine;

	CHECK(uniform01<float>(max_engine) < 1.0f);
	CHECK(uniform01<double>(max_engine) < 1.0);
	CHECK(uniformReal(max_engine, 0.0f, 1.0f) < 1.0f);
	CHECK(uniformReal(max_engine, -1e30f, 1e30f) < 1e30f);
}

TEST_CASE("'PiecewiseLinearDistribution' follows the density", "[tk::rng]")
{
	Pcg64 rng(3);

	SECTION("Triangle")
	{
		// Density 0 at 0, rising to the maximum at 1, mean is 2/3
		constexpr double POINTS[] = { 0.0, 1.0 };
		constexpr double WEIGHTS[] = { 0.0, 5.0 };
		PiecewiseLinearDistribution<double> dist(POINTS, WEIGHTS);

		double sum = 0.0;
		double min = 1.0;
		double max = 0.0;
		int below_half = 0;
		for (int i = 0; i < 100000; i++) {
			double value = dist(rng);
			min = std::min(min, value);
			max = std::max(max, value);
			sum += value;
			below_half += value < 0.5 ? 1 : 0;
		}

		CHECK(min >= 0.0);
		CHECK(max < 1.0);
		CHECK(sum / 100000.0 > 0.66);
		CHECK(sum / 100000.0 < 0.67);
		// P(x < 0.5) = 0.25, sigma is about 140
		CHECK(below_half > 24300);
		CHECK(below_half < 25700);
	}

	SECTION("Steps with a gap")
	{
		// Uniform on [0; 1] and [2; 3], nothing in between (zero density)
		constexpr float POINTS[] = { 0.0f, 1.0f, 1.0001f, 1.9999f, 2.0f, 3.0f };
		constexpr float WEIGHTS[] = { 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f };
		PiecewiseLinearDistribution<float> dist(POINTS, WEIGHTS);

		float min = 3.0f;
		float max = 0.0f;
		int low = 0;
		int in_gap = 0;
		for (int i = 0; i < 100000; i++) {
			float value = dist(rng);
			min = std::min(min, value);
			max = std::max(max, value);
			low += value < 1.5f ? 1 : 0;
			in_gap += value > 1.0001f && value < 1.9999f ? 1 : 0;
		}

		CHECK(min >= 0.0f);
		CHECK(max < 3.0f);
		CHECK(in_gap == 0);
		// Half of the samples, sigma is about 160
		CHECK(low > 49200);
		CHECK(low < 50800);
	}
}

TEST_CASE("Distributions are reproducible", "[tk::rng]")
{
	// These values lock the exact behavior of all distributions. They must not change
	// between platforms, compilers or optimization levels, otherwise anything seeded
	// (e.g. world generation) will produce different results. Change them only together
	// with a deliberate algorithm change, accepting that old seeds give new results.
	Pcg64 rng(12345);

	CHECK(generate<8>([&] { return uniformInt(rng, -100, 100); }) == std::to_array({ 64, 15, 57, 36, 78, 100, 71, 38 }));

	CHECK(generate<3>([&] { return uniformInt<uint64_t>(rng, 0, UINT64_MAX - 1); })
		== std::to_array<uint64_t>({ 0xd6725a2f2e176bf2, 0xd324ba3d9c16e483, 0xb0e1a0cf6f801586 }));

	CHECK(generate<6>([&] { return std::bit_cast<uint32_t>(uniformReal(rng, -1.5f, 2.5f)); })
		== std::to_array<uint32_t>({ 0x3fb69cc6, 0x3ed9a098, 0xbf0860d4, 0xbf86ef12, 0x3f822024, 0xbf9c57c2 }));

	CHECK(generate<4>([&] { return std::bit_cast<uint64_t>(uniformReal(rng, 10.0, 20.0)); })
		== std::to_array<uint64_t>({ 0x4032904fc7a438fc, 0x40316f93f9aa34af, 0x40325a66bc65876b, 0x402d614941b37ef6 }));

	constexpr float POINTS[] = { -1500.0f, -500.0f, -100.0f, 75.0f, 500.0f, 2500.0f, 5000.f };
	constexpr float WEIGHTS[] = { 1.0f, 10.0f, 20.0f, 35.0f, 20.0f, 7.5f, 1.0f };
	PiecewiseLinearDistribution<float> dist(POINTS, WEIGHTS);

	CHECK(generate<8>([&] { return std::bit_cast<uint32_t>(dist(rng)); })
		== std::to_array<uint32_t>(
			{ 0xc1babbe4, 0x450d02b0, 0xc21dbcda, 0x430ba341, 0xc3b75609, 0xc32ab016, 0x456174d6, 0x4535d906 }));

	CHECK(generate<16>([&] { return bernoulli(rng, 0.3); })
		== std::to_array<bool>({ 0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0 }));
}

TEST_CASE("'entropySeed' gives different values", "[tk::rng]")
{
	// Not a quality test, just ensure it's not some constant.
	// Two random 64-bit values colliding is practically impossible.
	CHECK(entropySeed() != entropySeed());
}

} // namespace tk::rng
