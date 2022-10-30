#pragma once

#include <cstddef>
#include <cstdint>

namespace voxen::terrain
{

// Provides constants for terrain subsystem, all in one place
class Config final {
public:
	Config() = delete;

	// --- Main parameters ---

	// Number of cells in the chunk. Must be a power of two.
	constexpr static uint32_t CHUNK_SIZE = 32;
	// Maximal LOD (inclusive) which a single chunk can have. Chunks with this
	// LOD value is called a "superchunk" and a uniform grid is made from them.
	constexpr static uint32_t CHUNK_MAX_LOD = 12;

	// --- LOD control parameters ---

	// The target angular diameter of a single chunk, LODs will be adjusted to
	// reach it. Decreasing this parameter will yield finer overall LODs.
	constexpr static double CHUNK_OPTIMAL_ANGULAR_SIZE_DEGREES = 50.0;
	// Maximal distance (measured in superchunks) from point of interest
	// to superchunk center which will trigger loading this superchunk.
	// The bigger it is, the more superchunks are loaded around POI.
	constexpr static double SUPERCHUNK_ENGAGE_FACTOR = 0.75;

	// --- Performance-tuning parameters ---

	// Maximal time, in ticks, after which non-updated point of interest will be discared
	constexpr static uint32_t POINT_OF_INTEREST_MAX_AGE = 1000;
	// Maximal time, in ticks, after which non-engaged superchunk will get unloaded
	constexpr static uint32_t SUPERCHUNK_MAX_AGE = 1000;
	// Maximal number of direct chunk changes which can happen during one tick.
	// This limit is a tradeoff between upper bound on a signle tick latency and throughput.
	constexpr static uint32_t TERRAIN_MAX_DIRECT_OP_COUNT = 64;
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
