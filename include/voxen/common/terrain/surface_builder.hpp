#pragma once

#include <voxen/common/terrain/chunk.hpp>

namespace voxen
{

class TerrainSurfaceBuilder {
public:
	static void calcSurface(const TerrainChunk::VoxelData& voxels, TerrainSurface& surface);
private:
	static double edgeOffsetX(const TerrainChunk::VoxelData& voxels, uint32_t i, uint32_t j, uint32_t k);
	static double edgeOffsetY(const TerrainChunk::VoxelData& voxels, uint32_t i, uint32_t j, uint32_t k);
	static double edgeOffsetZ(const TerrainChunk::VoxelData& voxels, uint32_t i, uint32_t j, uint32_t k);
};

}
