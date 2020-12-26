#include <voxen/common/terrain/loader.hpp>

#if VOXEN_DEBUG_BUILD == 1
#include <cassert>
#endif /* VOXEN_DEBUG_BUILD */

namespace voxen
{

static constexpr inline size_t CACHE_SIZE = 65536;

TerrainLoader::TerrainLoader() : m_cache(CACHE_SIZE)
#if VOXEN_DEBUG_BUILD == 1
, m_loaded_chunks(0, [](const TerrainChunkHeader& header) {return header.hash();})
#endif /* VOXEN_DEBUG_BUILD */
{
}

void TerrainLoader::load(const TerrainChunkHeader &header, TerrainChunkPrimaryData &output)
{
#if VOXEN_DEBUG_BUILD == 1
	auto search = m_loaded_chunks.find(header);
	// We mustn't load already loaded chunk
	assert(search == m_loaded_chunks.end());
	m_loaded_chunks.insert(header);
#endif /* VOXEN_DEBUG_BUILD */

	if (m_cache.tryFill(header, output))
		return;
	// TODO: support loading from disk
	m_generator.generate(header, output);
}

void TerrainLoader::unload(const TerrainChunk &chunk)
{
#if VOXEN_DEBUG_BUILD == 1
	TerrainChunkHeader header = chunk.header();
	auto search = m_loaded_chunks.find(header);
	// We mustn't unload not loaded yet chunk
	assert(search != m_loaded_chunks.end());
	m_loaded_chunks.erase(search);
#endif /* VOXEN_DEBUG_BUILD */

	// TODO: support saving to disk
	// insert will update chunk data, if the chunk still in cache
	// or insert again, if the cache already flush the chunk
	m_cache.insert(chunk);
}

}
