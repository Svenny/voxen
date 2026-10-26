#pragma once

#include <bit>
#include <cstdint>

// Hash and checksum utilities

namespace tk::hash
{

// Fixed-size (64-bit input) XXH64 with zero seed, can be useful to make
// well-distributed bits out of anything. XXH64 is bijective for 64-bit inputs
// so you can even directly compare hashes instead of keys <= 8 bytes.
constexpr uint64_t xxh64Fixed(uint64_t data) noexcept
{
	constexpr uint64_t PRIME1 = 11400714785074694791ULL;
	constexpr uint64_t PRIME2 = 14029467366897019727ULL;
	constexpr uint64_t PRIME3 = 1609587929392839161ULL;
	constexpr uint64_t PRIME4 = 9650029242287828579ULL;
	// `prime5` + fixed 8-byte size
	constexpr uint64_t INIT = 2870177450012600261ULL + 8ULL;

	// Perform one single 64-bit processing step
	data = std::rotl(data * PRIME2, 31) * PRIME1;
	uint64_t result = std::rotl(INIT ^ data, 27) * PRIME1 + PRIME4;
	result ^= result >> 33;
	result *= PRIME2;
	result ^= result >> 29;
	result *= PRIME3;
	result ^= result >> 32;
	// Note that all operations are easily reversible
	return result;
}

} // namespace tk::hash
