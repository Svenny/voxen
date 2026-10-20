module;

#include <bit>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <random>
#include <span>
#include <type_traits>
#include <vector>

module tk.rng;

namespace tk::rng
{

uint64_t entropySeed()
{
	std::random_device rd;
	// `random_device` returns 32-bit values
	const uint64_t hi = rd();
	const uint64_t lo = rd();
	// Mix in case the entropy source is weak
	return SplitMix64((hi << 32) | lo)();
}

} // namespace tk::rng
