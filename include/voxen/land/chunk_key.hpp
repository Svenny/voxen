#pragma once

#include <voxen/land/land_public_consts.hpp>
#include <voxen/util/hash.hpp>

#include <glm/vec3.hpp>

#include <bit>
#include <functional>

namespace voxen::land
{

// 64-bit packable chunk identifier with optional scale for aggregation/LOD.
// Useable as search key for associative containers.
//
// Number of bits for coordinate components limits the possible world size.
struct ChunkKey {
	ChunkKey() = default;

	// Construct from packed value, just a bit cast
	explicit ChunkKey(uint64_t packed) noexcept { *this = std::bit_cast<ChunkKey>(packed); }

	// Construct from unpacked chunk base position in chunk coordinates and log2(scale)
	explicit ChunkKey(glm::ivec3 base, uint32_t scale_log2 = 0) noexcept : scale_log2(scale_log2)
	{
		x = base.x;
		y = base.y;
		z = base.z;
	}

	bool operator==(const ChunkKey &other) const = default;
	bool operator!=(const ChunkKey &other) const = default;
	bool operator<(const ChunkKey &other) const noexcept { return packed() < other.packed(); }

	uint64_t packed() const noexcept { return std::bit_cast<uint64_t>(*this); }

	glm::ivec3 base() const noexcept { return glm::ivec3(x, y, z); }
	uint32_t scaleLog2() const noexcept { return scale_log2; }
	int32_t scaleMultiplier() const noexcept { return 1 << scale_log2; }

	// Return "parent" chunk key with LOD scale one level larger
	ChunkKey parentLodKey() const noexcept
	{
		uint64_t nscale = scale_log2 + 1;

		ChunkKey result;
		result.scale_log2 = nscale;
		result.x = (x >> nscale) << nscale;
		result.y = (y >> nscale) << nscale;
		result.z = (z >> nscale) << nscale;
		return result;
	}

	// Hash is bijective and guarantees no collisions
	uint64_t hash() const noexcept { return Hash::xxh64Fixed(packed()); }

	uint64_t scale_log2 : Consts::CHUNK_KEY_SCALE_BITS;
	int64_t x : Consts::CHUNK_KEY_XZ_BITS;
	int64_t y : Consts::CHUNK_KEY_Y_BITS;
	int64_t z : Consts::CHUNK_KEY_XZ_BITS;
};

} // namespace voxen::land

namespace std
{

template<>
struct hash<voxen::land::ChunkKey> {
	size_t operator()(const voxen::land::ChunkKey &ck) const noexcept { return size_t(ck.hash()); }
};

} // namespace std
