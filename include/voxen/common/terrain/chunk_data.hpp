#pragma once

#include <voxen/common/terrain/chunk_octree.hpp>
#include <voxen/common/terrain/hermite_data.hpp>
#include <voxen/common/terrain/surface.hpp>

#include <glm/glm.hpp>

#include <array>
#include <vector>

namespace voxen
{

struct TerrainChunkPrimaryData {
public:
	// Must be a power of two not exceeding 128
	static constexpr inline uint32_t GRID_CELL_COUNT = 32;
	static constexpr inline uint32_t GRID_VERTEX_COUNT = GRID_CELL_COUNT + 1;

	using VoxelsArray = // 3D array with YXZ layout
		std::array<std::array<std::array<voxel_t, GRID_VERTEX_COUNT>, GRID_VERTEX_COUNT>, GRID_VERTEX_COUNT>;

	TerrainChunkPrimaryData() = default;
	TerrainChunkPrimaryData(TerrainChunkPrimaryData &&) = default;
	TerrainChunkPrimaryData(const TerrainChunkPrimaryData &) = default;
	TerrainChunkPrimaryData &operator = (TerrainChunkPrimaryData &&) = default;
	TerrainChunkPrimaryData &operator = (const TerrainChunkPrimaryData &) = default;
	~TerrainChunkPrimaryData() = default;

	// Compare two primary data storages for contents equality.
	// NOTE: VERY SLOW, use only when really needed (preferably for debug only).
	bool operator == (const TerrainChunkPrimaryData &other) const noexcept;

	std::array<voxel_t, 8> materialsOfCell(glm::uvec3 cell) const noexcept;

	VoxelsArray voxels;
	HermiteDataStorage hermite_data_x, hermite_data_y, hermite_data_z;
};

struct TerrainChunkSecondaryData {
public:
	TerrainChunkSecondaryData() = default;
	TerrainChunkSecondaryData(TerrainChunkSecondaryData &&) = default;
	TerrainChunkSecondaryData(const TerrainChunkSecondaryData &) = default;
	TerrainChunkSecondaryData &operator = (TerrainChunkSecondaryData &&) = default;
	TerrainChunkSecondaryData &operator = (const TerrainChunkSecondaryData &) = default;
	~TerrainChunkSecondaryData() = default;

	bool operator == (const TerrainChunkSecondaryData &other) const noexcept;

	ChunkOctree octree;
	TerrainSurface surface;
};

}
