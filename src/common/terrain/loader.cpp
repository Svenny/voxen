#include <voxen/common/terrain/loader.hpp>

#if VOXEN_DEBUG_BUILD == 1
#include <cassert>
#include <algorithm>
#endif /* VOXEN_DEBUG_BUILD */

namespace voxen
{

static constexpr inline size_t CACHE_SIZE = 65536;

TerrainLoader::TerrainLoader() : m_cache(CACHE_SIZE)
{
}

void TerrainLoader::load(TerrainChunk &chunk)
{
	if (m_cache.tryFill(chunk))
		return;
	// TODO: support loading from disk
	m_generator.generate(chunk);
	m_cache.insert(chunk);

#if VOXEN_DEBUG_BUILD == 1
	TerrainChunkHeader header = chunk.header();
	auto search = std::find(m_loaded_chunks.begin(), m_loaded_chunks.end(), header);
	// We mustn't load already loaded chunk
	assert(search == m_loaded_chunks.end());
	m_loaded_chunks.push_back(header);
#endif /* VOXEN_DEBUG_BUILD */
}

void TerrainLoader::unload(TerrainChunk &chunk)
{
	// TODO: support saving to disk

	// insert will update chunk data, if the chunk still in cache
	// or insert again, if the cache already flush the chunk
	m_cache.insert(chunk);
	(void) chunk;
#if VOXEN_DEBUG_BUILD == 1
	TerrainChunkHeader header = chunk.header();
	auto search = std::find(m_loaded_chunks.begin(), m_loaded_chunks.end(), header);
	// We mustn't unload not loaded yet chunk
	assert(search != m_loaded_chunks.end());
	m_loaded_chunks.erase(search);
#endif /* VOXEN_DEBUG_BUILD */
}

}
