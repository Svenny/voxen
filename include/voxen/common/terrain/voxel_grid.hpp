#pragma once

#include <voxen/common/terrain/config.hpp>

#include <array>

namespace voxen::terrain
{

// A uniform 3D grid of voxels.
// NOTE: this class is very big and should not be allocated on stack.
class VoxelGrid final {
public:
	// Adding 1 because N cells require N+1 grid points
	constexpr static uint32_t GRID_SIZE = Config::CHUNK_SIZE + 1;

	// A single Z scanline of the grid
	using VoxelsScanline = std::array<voxel_t, GRID_SIZE>;
	// A single Y plane of the grid, array has an XZ layout
	using VoxelsPlane = std::array<VoxelsScanline, GRID_SIZE>;
	// Array has an YXZ layout
	using VoxelsArray = std::array<VoxelsPlane, GRID_SIZE>;

	VoxelGrid() = default;
	VoxelGrid(VoxelGrid &&) = default;
	VoxelGrid(const VoxelGrid &) = default;
	VoxelGrid &operator=(VoxelGrid &&) = default;
	VoxelGrid &operator=(const VoxelGrid &) = default;
	~VoxelGrid() = default;

	// Return linearized array of voxels from cell `(x, y, z):(x+1, y+1, z+1)`.
	// Linearized array follows standard "octree children" (YXZ) ordering.
	std::array<voxel_t, 8> getCellLinear(uint32_t x, uint32_t y, uint32_t z) const noexcept;

	VoxelsArray &voxels() noexcept { return m_data; }
	const VoxelsArray &voxels() const noexcept { return m_data; }

	// Return an array of voxels in XZ plane with given Y value
	VoxelsPlane &yPlane(uint32_t y) noexcept;
	const VoxelsPlane &yPlane(uint32_t y) const noexcept;

	// Return an array of voxels along Z axis with given X and Y values
	VoxelsScanline &zScanline(uint32_t x, uint32_t y) noexcept;
	const VoxelsScanline &zScanline(uint32_t x, uint32_t y) const noexcept;

private:
	VoxelsArray m_data;
};

} // namespace voxen::terrain
