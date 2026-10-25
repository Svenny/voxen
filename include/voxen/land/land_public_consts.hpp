#pragma once

#include <cstdint>

// Publicly accessible constants of Land subsystem
namespace voxen::land::Consts
{

// World-space size of a single block
constexpr double BLOCK_SIZE_METRES = 0.75;

// How many blocks fit in one chunk with scale 1, per axis
constexpr int32_t CHUNK_SIZE_BLOCKS = 32;

// World-space size of a single chunk
constexpr double CHUNK_SIZE_METRES = BLOCK_SIZE_METRES * CHUNK_SIZE_BLOCKS;

// Limits the maximal number of chunks available in X/Z axes. Must be less than 32 bits.
constexpr uint32_t CHUNK_KEY_XZ_BITS = 22;
// Limits the maximal number of chunks available in Y axis. Must be less than 32 bits.
constexpr uint32_t CHUNK_KEY_Y_BITS = 16;
// Limits the maximal chunk aggregation (LOD) level.
// Must be enough to fit every scale from 0 to `NUM_LOD_SCALES - 1`.
constexpr uint32_t CHUNK_KEY_SCALE_BITS = 4;

// Number of available chunk aggregation (LOD) levels and their world size
// scale multipliers (powers of two - 1, 2, 4, 8, ..., 2^(NUM_LOD_SCALES-1)).
// World height and X/Z tile size below are derived from it.
constexpr uint32_t NUM_LOD_SCALES = 9;

// The maximal chunk scale (LOD) that can be directly generated. Several highest levels
// are too sparse (too low resolution) for it and can be obtained only by aggregating
// data generated at this level. If land generation performance becomes a bottleneck
// this limit can be raised - actually I wouldn't expect major quality degragation.
constexpr uint32_t MAX_GENERATABLE_LOD = NUM_LOD_SCALES - 3;

// Size, in chunks, of a single world tile in X/Z axes. World X/Z extent
// is a whole number of tiles, each spanning 64 chunks of the largest LOD.
constexpr uint32_t WORLD_TILE_SIZE_CHUNKS = 1u << (NUM_LOD_SCALES - 1 + 3 + 3);
// Number of world tiles in X axis. Must be even.
// As tile size is fixed, this knob effectively controls planet size in longitude.
constexpr uint32_t WORLD_SIZE_X_TILES = 24;
// Number of world tiles in Z axis. Must be even.
// As tile size is fixed, this knob effectively controls planet size in latitude.
constexpr uint32_t WORLD_SIZE_Z_TILES = 4;

// The lowest chunk Y (altitude) coordinate that can exist in the world.
// Chunks below it are never generated or loaded.
constexpr int32_t MIN_WORLD_Y_CHUNK = -int32_t(1u << (NUM_LOD_SCALES - 1));
// The highest chunk Y (altitude) coordinate that can exist in the world.
// Chunks above it are never generated or loaded.
constexpr int32_t MAX_WORLD_Y_CHUNK = -MIN_WORLD_Y_CHUNK - 1;

// Minimal chunk X (longitude) coordinate before wraparound begins
constexpr int32_t MIN_UNIQUE_WORLD_X_CHUNK = -int32_t(WORLD_TILE_SIZE_CHUNKS * WORLD_SIZE_X_TILES / 2);
// Maximal chunk X (longitude) coordinate before wraparound begins
constexpr int32_t MAX_UNIQUE_WORLD_X_CHUNK = -MIN_UNIQUE_WORLD_X_CHUNK - 1;

// Minimal chunk Z (latitude) coordinate before wraparound begins
constexpr int32_t MIN_UNIQUE_WORLD_Z_CHUNK = -int32_t(WORLD_TILE_SIZE_CHUNKS * WORLD_SIZE_Z_TILES / 2);
// Maximal chunk Z (latitude) coordinate before wraparound begins
constexpr int32_t MAX_UNIQUE_WORLD_Z_CHUNK = -MIN_UNIQUE_WORLD_Z_CHUNK - 1;

} // namespace voxen::land::Consts
