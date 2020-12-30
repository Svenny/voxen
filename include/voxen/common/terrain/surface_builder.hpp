#pragma once

#include <voxen/common/terrain/chunk_data.hpp>

namespace voxen
{

class TerrainSurfaceBuilder {
public:
	static void buildBasicOctree(const TerrainChunkPrimaryData &input, TerrainChunkSecondaryData &output);
	static void buildSurface(TerrainChunkSecondaryData &output);
};

}
