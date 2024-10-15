#pragma once

#include <cstdint>

// Publicly accessible constants of Land subsystem
namespace voxen::land::Consts
{

// World-space size of a single block
constexpr double BLOCK_SIZE_METRES = 0.75;

// How many blocks fit in one chunk with scale 1, per axis
constexpr int32_t CHUNK_SIZE_BLOCKS = 32;
constexpr double CHUNK_SIZE_METRES = BLOCK_SIZE_METRES * CHUNK_SIZE_BLOCKS;

// Limits the maximal number of chunks available in X/Z axes
constexpr uint32_t CHUNK_KEY_XZ_BITS = 22;
// Limits the maximal number of chunks available in Y axis
constexpr uint32_t CHUNK_KEY_Y_BITS = 16;
// Limits the maximal chunk aggregation (LOD) level
constexpr uint32_t CHUNK_KEY_SCALE_BITS = 4;

constexpr uint32_t REGION_SIZE_LOG2_CHUNKS = 4;
constexpr int32_t REGION_SIZE_CHUNKS = 1 << REGION_SIZE_LOG2_CHUNKS;
constexpr int32_t REGION_SIZE_BLOCKS = CHUNK_SIZE_BLOCKS * REGION_SIZE_CHUNKS;
constexpr double REGION_SIZE_METRES = BLOCK_SIZE_METRES * REGION_SIZE_BLOCKS;

constexpr uint32_t NUM_IMPOSTOR_LEVELS = REGION_SIZE_LOG2_CHUNKS + 1;
constexpr uint32_t NUM_LOD_LEVELS = NUM_IMPOSTOR_LEVELS + 1;
constexpr uint32_t IMPOSTOR_SIZE_PIXELS = 32;

} // namespace voxen::land::Consts
