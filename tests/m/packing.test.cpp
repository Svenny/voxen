#include <m/m.hpp>

#include "../test_common.hpp"

namespace m
{

namespace
{

// Halfway cases round to even: 127.5 -> 128, 32767.5 -> 32768, 2147483647.5 -> 2147483648
static_assert(packUnorm<uint8_t>(1.0f) == 255 && packUnorm<uint8_t>(0.0f) == 0);
static_assert(packUnorm<uint8_t>(2.0f) == 255 && packUnorm<uint8_t>(-1.0f) == 0);
static_assert(packUnorm<uint8_t>(0.5f) == 128);
static_assert(packUnorm<uint16_t>(float3(0, 0.5f, 1)) == uint16_t3(0, 32768, 65535));
static_assert(packUnorm<uint32_t>(1.0f) == 0xFFFFFFFFu && packUnorm<uint32_t>(0.5) == 0x80000000u);
static_assert(packUnorm<uint8_t>(double4(0.1, 0.2, 0.3, 0.999)) == uint8_t4(26, 51, 76, 255));

static_assert(packSnorm<int8_t>(float2(-1, 1)) == int8_t2(-127, 127));
static_assert(packSnorm<int8_t>(-2.0f) == -127 && packSnorm<int8_t>(2.0f) == 127);
static_assert(packSnorm<int16_t>(0.0f) == 0 && packSnorm<int16_t>(0.5f) == 16384);
static_assert(packSnorm<int32_t>(-1.0) == -0x7FFFFFFF);

static_assert(unpackUnorm<float>(uint8_t(255)) == 1.0f);
static_assert(unpackUnorm<float>(uint8_t3(0, 255, 51)) == float3(0, 1, 0.2f));
static_assert(unpackUnorm<double>(uint16_t(65535)) == 1.0);
static_assert(unpackSnorm<float>(int8_t(127)) == 1.0f && unpackSnorm<float>(int8_t(-127)) == -1.0f);
// The most negative value clamps to -1
static_assert(unpackSnorm<float>(int8_t(-128)) == -1.0f);
static_assert(unpackSnorm<double>(int16_t2(0, -32767)) == double2(0, -1));

} // namespace

TEST_CASE("Normalized integers round-trip through floats", "[m::packing]")
{
	size_t mismatches = 0;
	for (int i = 0; i < 256; i++) {
		const auto u = uint8_t(i);
		mismatches += packUnorm<uint8_t>(unpackUnorm<float>(u)) != u;
		mismatches += packUnorm<uint8_t>(unpackUnorm<double>(u)) != u;
	}
	for (int i = -127; i < 128; i++) {
		const auto s = int8_t(i);
		mismatches += packSnorm<int8_t>(unpackSnorm<float>(s)) != s;
	}
	// 16-bit values use the same code paths, sample them
	for (int i = 0; i < 65536; i += 37) {
		const auto u = uint16_t(i);
		const auto s = int16_t(i - 32768 + 1);
		mismatches += packUnorm<uint16_t>(unpackUnorm<float>(u)) != u;
		mismatches += packSnorm<int16_t>(unpackSnorm<float>(s)) != s;
	}
	CHECK(mismatches == 0);
}

TEST_CASE("Packing error is at most half a step", "[m::packing]")
{
	const float inputs[] = { 0.0f, 0.1f, 0.123456f, 0.5f, 0.7777f, 0.99999f, 1.0f };
	float max_error = 0;
	for (float x : inputs) {
		max_error = max(max_error, abs(unpackUnorm<float>(packUnorm<uint16_t>(x)) - x));
		max_error = max(max_error, abs(unpackSnorm<float>(packSnorm<int16_t>(x)) - x));
		max_error = max(max_error, abs(unpackSnorm<float>(packSnorm<int16_t>(-x)) + x));
	}
	CHECK(max_error <= 0.5f / 32767.0f);
}

} // namespace m
