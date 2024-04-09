#include <voxen/common/terrain/primary_data.hpp>

namespace voxen::terrain
{

void ChunkPrimaryData::clear() noexcept
{
	hermite_data_x.clear();
	hermite_data_y.clear();
	hermite_data_z.clear();
}

} // namespace voxen::terrain
