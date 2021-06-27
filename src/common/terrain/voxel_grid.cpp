#include <voxen/common/terrain/voxel_grid.hpp>

#include <cassert>

namespace voxen::terrain
{

std::array<voxel_t, 8> VoxelGrid::getCellLinear(uint32_t x, uint32_t y, uint32_t z) const noexcept
{
	assert(x < GRID_SIZE);
	assert(y < GRID_SIZE);
	assert(z < GRID_SIZE);

	std::array<voxel_t, 8> result;
	result[0] = m_data[y][x][z];
	result[1] = m_data[y][x][z+1];
	result[2] = m_data[y][x+1][z];
	result[3] = m_data[y][x+1][z+1];
	result[4] = m_data[y+1][x][z];
	result[5] = m_data[y+1][x][z+1];
	result[6] = m_data[y+1][x+1][z];
	result[7] = m_data[y+1][x+1][z+1];
	return result;
}

VoxelGrid::VoxelsPlane &VoxelGrid::yPlane(uint32_t y) noexcept
{
	assert(y < GRID_SIZE);
	return m_data[y];
}

const VoxelGrid::VoxelsPlane &VoxelGrid::yPlane(uint32_t y) const noexcept
{
	assert(y < GRID_SIZE);
	return m_data[y];
}

VoxelGrid::VoxelsScanline &VoxelGrid::zScanline(uint32_t x, uint32_t y) noexcept
{
	assert(x < GRID_SIZE);
	assert(y < GRID_SIZE);
	return m_data[y][x];
}

const VoxelGrid::VoxelsScanline &VoxelGrid::zScanline(uint32_t x, uint32_t y) const noexcept
{
	assert(x < GRID_SIZE);
	assert(y < GRID_SIZE);
	return m_data[y][x];
}

}
