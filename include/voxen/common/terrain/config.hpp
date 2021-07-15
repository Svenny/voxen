#pragma once

#include <cstddef>
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
	// Recently unloaded chunks are stored into a cache to allow fast reloading
	// (imagine "going back and forth" scenario). This cache is set-associative.
	constexpr static size_t CHUNK_CACHE_SET_SIZE = 8;
	// How much chunks can standby cache hold in theory. Note that due to set
	// associativity it may start evictions even when not being fully occupied.
	// Also note actual cache capacity may be slightly greater due to round-off.
	constexpr static size_t CHUNK_CACHE_FULL_SIZE = 65536;
};

// Alias for voxel ID storage type
using voxel_t = uint8_t;

// Alias for chunk version storage type.
// Version is assumed to be strictly increased after each change to chunk contents. So logic will break
// completely when it gets past UINT32_MAX (thus wrapping to zero), but we don't expect any real-world
// runtime to ever reach values that large (more than 4 billion edits of a single chunk).
using chunk_ver_t = uint32_t;

}
