module;

#include <modshared/api.hpp>

#include <cstdint>
#include <string>

module modshared;

import tk.rng;

namespace modshared
{

int32_t g_calls = 0;

Counter::~Counter() = default;

void Counter::add(int32_t value) noexcept
{
	g_calls++;
	m_value += value;
}

std::string Counter::name() const
{
	return "Counter";
}

m::float3 scaled(const m::float3 &v, float s) noexcept
{
	g_calls++;
	return v * s;
}

uint64_t seededRandom(uint64_t seed) noexcept
{
	g_calls++;
	return tk::rng::SplitMix64(seed)();
}

} // namespace modshared
