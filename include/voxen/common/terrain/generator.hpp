#pragma once

#include <voxen/common/terrain/primary_data.hpp>
#include <voxen/land/chunk_key.hpp>

namespace voxen::terrain
{

class TerrainGenerator final {
public:
	void generate(land::ChunkKey id, ChunkPrimaryData &output) const;
};

} // namespace voxen::terrain
