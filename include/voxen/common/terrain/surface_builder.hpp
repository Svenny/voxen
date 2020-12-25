#pragma once

#include <voxen/common/terrain/chunk_data.hpp>

namespace voxen
{

class TerrainSurfaceBuilder {
public:
	static void calcSurface(const TerrainChunkPrimaryData &input, TerrainChunkSecondaryData &output);
};

}
