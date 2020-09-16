#pragma once

#include <voxen/common/terrain/chunk.hpp>

namespace voxen
{

class TerrainSurfaceBuilder {
public:
	static void calcSurface(const TerrainChunk::Data& chunk_data, TerrainSurface& surface);
private:
	static double edgeOffset(double d1, double d2);
};

}
