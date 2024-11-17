#include <voxen/common/terrain/primary_data.hpp>

namespace voxen::terrain
{

void ChunkPrimaryData::clear() noexcept
{
	hermite_data.clear();
}

} // namespace voxen::terrain
