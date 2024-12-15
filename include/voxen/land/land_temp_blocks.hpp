#pragma once

#include <voxen/land/land_chunk.hpp>
#include <voxen/util/packed_color.hpp>

namespace voxen::land
{

// Temporary hardcoded interface for block metadata.
// Until I implement the proper "registry" interface.
class TempBlockMeta {
public:
	enum Block : Chunk::BlockId {
		BlockEmpty = 0,
		BlockUnderlimit = 1,

		BlockStone = 2,
		BlockGrass = 3,
		BlockSand = 4,
		BlockSnow = 5,
		BlockWater = 6,
		BlockDirt = 7,

		BlockCount
	};

	constexpr static Chunk::BlockId NUM_BLOCKS = BlockCount;

	constexpr static PackedColorSrgb BLOCK_FIXED_COLOR[NUM_BLOCKS] = {
		{ 0, 0, 0, 0 }, // Empty
		{ 10, 10, 15 }, // Underlimit

		{ 120, 120, 120 }, // Stone
		{ 50, 160, 50 },   // Grass
		{ 210, 180, 140 }, // Sand
		{ 240, 240, 250 }, // Snow
		{ 30, 120, 200 },  // Water
		{ 90, 60, 30 },    // Dirt
	};

	constexpr static const char *BLOCK_NAME[NUM_BLOCKS] = {
		"Empty",
		"Underlimit",

		"Stone",
		"Grass",
		"Sand",
		"Snow",
		"Water",
		"Dirt",
	};

	static bool isBlockEmpty(Chunk::BlockId id) noexcept
	{
		return id == 0;
	}

	static uint16_t packColor555(PackedColorSrgb color) noexcept
	{
		// Set "fixed color" flag
		int32_t packer = 1 << 15;

		packer |= ((color.r + 4) >> 3) << 10;
		packer |= ((color.g + 4) >> 3) << 5;
		packer |= ((color.b + 4) >> 3);

		return static_cast<uint16_t>(packer);
	}
};

} // namespace voxen::land
