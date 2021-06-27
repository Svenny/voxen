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
};

// Alias for voxel ID storage type
using voxel_t = uint8_t;

}

namespace voxen
{

// TODO: remove this deprecated alias
using voxel_t = terrain::voxel_t;

}
