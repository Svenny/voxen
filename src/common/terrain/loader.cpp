#include <voxen/common/terrain/loader.hpp>

#include <voxen/common/terrain/allocator.hpp>
#include <voxen/common/terrain/surface_builder.hpp>

#include <extras/defer.hpp>

#include <cassert>

namespace voxen::terrain
{

void TerrainLoader::load(Chunk &chunk)
{
	const ChunkId id = chunk.id();

#if VOXEN_DEBUG_BUILD == 1
	{
		std::lock_guard<std::mutex> lock(m_access_mutex);
		auto iter = m_loaded_chunks.find(id);
		// We must not load an already loaded chunk
		assert(iter == m_loaded_chunks.end());
		m_loaded_chunks.insert(id);
	}

	defer_fail {
		// Rollback in case of load failure
		std::lock_guard<std::mutex> lock(m_access_mutex);
		m_loaded_chunks.erase(id);
	};
#endif /* VOXEN_DEBUG_BUILD */

	{
		std::lock_guard<std::mutex> lock(m_access_mutex);
		if (m_cache.tryLoad(chunk)) {
			// Standby cache hit
			return;
		}
	}

	// TODO: support loading from disk
	m_generator.generate(id, chunk.primaryData());
	SurfaceBuilder::buildSurface(chunk);
}

void TerrainLoader::unload(extras::refcnt_ptr<Chunk> chunk)
{
	assert(chunk);

	std::lock_guard<std::mutex> lock(m_access_mutex);

#if VOXEN_DEBUG_BUILD == 1
	{
		auto iter = m_loaded_chunks.find(chunk->id());
		// We must not unload not yet loaded chunk
		assert(iter != m_loaded_chunks.end());
		m_loaded_chunks.erase(iter);
	}
#endif /* VOXEN_DEBUG_BUILD */

	m_cache.insert(std::move(chunk));

	// TODO: support saving to disk
	// Currently chunks are just dropped
}

}
