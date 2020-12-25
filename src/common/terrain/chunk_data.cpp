#include <voxen/common/terrain/chunk_data.hpp>

namespace voxen
{

bool TerrainChunkPrimaryData::operator == (const TerrainChunkPrimaryData &other) const noexcept
{
	return voxels == other.voxels &&
		hermite_data_x == other.hermite_data_x &&
		hermite_data_y == other.hermite_data_y &&
		hermite_data_z == other.hermite_data_z;
}

std::array<voxel_t, 8> TerrainChunkPrimaryData::materialsOfCell(glm::uvec3 cell) const noexcept
{
	// Voxels data - YXZ storage
	const uint32_t y = cell.y;
	const uint32_t x = cell.x;
	const uint32_t z = cell.z;
	assert(x < GRID_CELL_COUNT);
	assert(y < GRID_CELL_COUNT);
	assert(z < GRID_CELL_COUNT);

	std::array<voxel_t, 8> mats;
	mats[0] = voxels[y][x][z];
	mats[1] = voxels[y][x][z+1];
	mats[2] = voxels[y][x+1][z];
	mats[3] = voxels[y][x+1][z+1];
	mats[4] = voxels[y+1][x][z];
	mats[5] = voxels[y+1][x][z+1];
	mats[6] = voxels[y+1][x+1][z];
	mats[7] = voxels[y+1][x+1][z+1];
	return mats;
}

}
