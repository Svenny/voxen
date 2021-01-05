#pragma once

#include <voxen/common/terrain/cache.hpp>
#include <voxen/common/terrain/chunk.hpp>
#include <voxen/common/terrain/generator.hpp>

#include <mutex>

#if VOXEN_DEBUG_BUILD == 1
#include <unordered_set>
#include <functional>
#endif /* VOXEN_DEBUG_BUILD */

namespace voxen
{

// This class supports access from multiple threads
class TerrainLoader {
public:
	TerrainLoader();

	void load(TerrainChunk &chunk);
	void unload(const TerrainChunk &chunk);
private:
	std::mutex m_access_mutex;
	TerrainChunkCache m_cache;
	const TerrainGenerator m_generator;
#if VOXEN_DEBUG_BUILD == 1
	std::unordered_set<TerrainChunkHeader, std::function<uint64_t(const TerrainChunkHeader&)>> m_loaded_chunks;
#endif /* VOXEN_DEBUG_BUILD */
};

}
