#pragma once

#include <voxen/common/terrain/chunk_header.hpp>
#include <voxen/common/terrain/chunk_id.hpp>
#include <voxen/common/terrain/primary_data.hpp>

namespace voxen::terrain
{

class TerrainGenerator final {
public:

	void generate(ChunkId id, ChunkPrimaryData &output) const;
	// TODO: remove this deprecated function
	void generate(const TerrainChunkHeader &header, ChunkPrimaryData &output) const;
private:

};

}
