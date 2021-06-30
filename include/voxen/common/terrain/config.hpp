#pragma once

#include <cstdint>

namespace voxen::terrain
{

// Provides constants for terrain subsystem, all in one place
class Config final {
public:
	Config() = delete;

	// Number of cells in the chunk. Must be a power of two.
	constexpr static uint32_t CHUNK_SIZE = 32;
	// Allocation of terrain-related entities is done through object pools,
	// which are in turn composed of subpools - contiguous storages of fixed size.
	// This constant determines the size of a single subpool. The larger it is,
	// the faster is allocation, but it also increases lower bound on memory
	// usage (causing memory overhead/waste when storage is underutilized).
	constexpr static uint32_t ALLOCATION_SUBPOOL_SIZE = 512;
};

// Alias for voxel ID storage type
using voxel_t = uint8_t;

// Alias for chunk version storage type.
// Version is assumed to be strictly increased after each change to chunk contents. So logic will break
// completely when it gets past UINT32_MAX (thus wrapping to zero), but we don't expect any real-world
// runtime to ever reach values that large (more than 4 billion edits of a single chunk).
using chunk_ver_t = uint32_t;

}

namespace voxen
{

// TODO: remove this deprecated alias
using voxel_t = terrain::voxel_t;

}
