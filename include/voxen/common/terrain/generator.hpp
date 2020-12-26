#pragma once

#include <voxen/common/terrain/chunk_data.hpp>
#include <voxen/common/terrain/chunk_header.hpp>

namespace voxen
{

class TerrainGenerator {
public:

	void generate(const TerrainChunkHeader &header, TerrainChunkPrimaryData &output);
private:

};

}
