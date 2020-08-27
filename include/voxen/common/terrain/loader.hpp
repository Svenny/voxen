#pragma once

#include <voxen/common/terrain/cache.hpp>
#include <voxen/common/terrain/chunk.hpp>
#include <voxen/common/terrain/generator.hpp>

namespace voxen
{

class TerrainLoader {
public:
	TerrainLoader();

	void load(TerrainChunk &chunk);
	void unload(TerrainChunk &chunk);
private:
	TerrainChunkCache m_cache;
	TerrainGenerator m_generator;
};

}
