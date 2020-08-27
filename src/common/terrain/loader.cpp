#include <voxen/common/terrain/loader.hpp>

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
}

void TerrainLoader::unload(TerrainChunk &chunk)
{
	// TODO: support saving to disk
	(void) chunk;
}

}
