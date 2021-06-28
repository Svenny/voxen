#pragma once

#include <voxen/common/terrain/chunk_octree.hpp>
#include <voxen/common/terrain/primary_data.hpp>
#include <voxen/common/terrain/surface.hpp>

// TODO: remove this deprecated header
namespace voxen
{

// TODO: remove this deprecated alias
using TerrainChunkPrimaryData = terrain::ChunkPrimaryData;

struct TerrainChunkSecondaryData {
public:
	TerrainChunkSecondaryData() = default;
	TerrainChunkSecondaryData(TerrainChunkSecondaryData &&) = default;
	TerrainChunkSecondaryData(const TerrainChunkSecondaryData &) = default;
	TerrainChunkSecondaryData &operator = (TerrainChunkSecondaryData &&) = default;
	TerrainChunkSecondaryData &operator = (const TerrainChunkSecondaryData &) = default;
	~TerrainChunkSecondaryData() = default;

	bool operator == (const TerrainChunkSecondaryData &other) const noexcept;

	terrain::ChunkOctree octree;
	terrain::ChunkOwnSurface surface;
};

}
