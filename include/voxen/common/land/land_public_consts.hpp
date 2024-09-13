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

} // namespace voxen::land::Consts
