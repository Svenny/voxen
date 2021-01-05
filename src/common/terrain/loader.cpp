#include <voxen/common/terrain/loader.hpp>

#include <voxen/common/terrain/surface_builder.hpp>

#if VOXEN_DEBUG_BUILD == 1
#include <cassert>
#endif /* VOXEN_DEBUG_BUILD */

namespace voxen
{

static constexpr inline size_t CACHE_SIZE = 65536;

TerrainLoader::TerrainLoader() : m_cache(CACHE_SIZE), m_generator()
#if VOXEN_DEBUG_BUILD == 1
, m_loaded_chunks(0, [](const TerrainChunkHeader& header) {return header.hash();})
#endif /* VOXEN_DEBUG_BUILD */
{
}

void TerrainLoader::load(TerrainChunk &chunk)
{
	const auto &header = chunk.header();
#if VOXEN_DEBUG_BUILD == 1
	m_access_mutex.lock();
	auto search = m_loaded_chunks.find(header);
	// We mustn't load already loaded chunk
	assert(search == m_loaded_chunks.end());
	m_loaded_chunks.insert(header);
	m_access_mutex.unlock();
#endif /* VOXEN_DEBUG_BUILD */

	m_access_mutex.lock();
	if (m_cache.tryFill(chunk))
	{
		m_access_mutex.unlock();
		return;
	}
	m_access_mutex.unlock();
	// TODO: support loading from disk
	auto[primary, secondary] = chunk.beginEdit();
	m_generator.generate(header, primary);
	TerrainSurfaceBuilder::buildBasicOctree(primary, secondary);
	chunk.endEdit();
}

void TerrainLoader::unload(const TerrainChunk &chunk)
{
#if VOXEN_DEBUG_BUILD == 1
	TerrainChunkHeader header = chunk.header();
	m_access_mutex.lock();
	auto search = m_loaded_chunks.find(header);
	// We mustn't unload not loaded yet chunk
	assert(search != m_loaded_chunks.end());
	m_loaded_chunks.erase(search);
	m_access_mutex.unlock();
#endif /* VOXEN_DEBUG_BUILD */

	// TODO: support saving to disk
	// insert will update chunk data, if the chunk still in cache
	// or insert again, if the cache already flush the chunk
	m_access_mutex.lock();
	m_cache.insert(chunk);
	m_access_mutex.unlock();
}

}
