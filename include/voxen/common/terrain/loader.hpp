#pragma once

#include <voxen/common/terrain/cache.hpp>
#include <voxen/common/terrain/chunk.hpp>
#include <voxen/common/terrain/generator.hpp>

#if VOXEN_DEBUG_BUILD == 1
#include <unordered_set>
#include <functional>
#endif /* VOXEN_DEBUG_BUILD */

namespace voxen
{

class TerrainLoader {
public:
	TerrainLoader();

	void load(const TerrainChunkHeader &header, TerrainChunkPrimaryData &output);
	void unload(const TerrainChunk &chunk);
private:
	TerrainChunkCache m_cache;
	TerrainGenerator m_generator;
#if VOXEN_DEBUG_BUILD == 1
	std::unordered_set<TerrainChunkHeader, std::function<uint64_t(const TerrainChunkHeader&)>> m_loaded_chunks;
#endif /* VOXEN_DEBUG_BUILD */
};

}
