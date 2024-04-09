#pragma once

#include <voxen/common/terrain/hermite_data.hpp>
#include <voxen/common/terrain/voxel_grid.hpp>

namespace voxen::terrain
{

// A pack of uniform voxel grid and its associated Hermite data storages.
// NOTE: this struct is very big and should not be allocated on stack.
struct ChunkPrimaryData final {
	VoxelGrid voxel_grid;
	HermiteDataStorage hermite_data_x;
	HermiteDataStorage hermite_data_y;
	HermiteDataStorage hermite_data_z;

	void clear() noexcept;
};

} // namespace voxen::terrain
