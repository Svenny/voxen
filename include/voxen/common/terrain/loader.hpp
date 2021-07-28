#pragma once

#include <voxen/common/terrain/cache.hpp>
#include <voxen/common/terrain/chunk.hpp>
#include <voxen/common/terrain/generator.hpp>

#include <mutex>

#if VOXEN_DEBUG_BUILD == 1
#include <unordered_set>
#endif /* VOXEN_DEBUG_BUILD */

namespace voxen::terrain
{

// This class supports access from multiple threads
class TerrainLoader {
public:
	void load(Chunk &chunk);
	void unload(extras::refcnt_ptr<Chunk> chunk);

private:
	std::mutex m_access_mutex;
	ChunkCache m_cache;
	TerrainGenerator m_generator;

#if VOXEN_DEBUG_BUILD == 1
	std::unordered_set<ChunkId> m_loaded_chunks;
#endif /* VOXEN_DEBUG_BUILD */
};

}
