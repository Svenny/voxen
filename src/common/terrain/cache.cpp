#include <voxen/common/terrain/cache.hpp>

#include <voxen/util/log.hpp>

#include <cassert>
#include <algorithm>

namespace voxen
{

TerrainChunkCache::TerrainChunkCache(size_t max_chunks)
	: m_sets((max_chunks + SET_SIZE - 1) / SET_SIZE)
{
	Log::debug("Creating terrain chunk cache for {} max chunks - using {} sets", max_chunks, m_sets.size());
}

bool TerrainChunkCache::tryFill(TerrainChunk &chunk)
{
	auto[set_id, chunk_pos_in_set] = findSetAndIndex(chunk);
	if (chunk_pos_in_set == SET_SIZE) {
		// Not found
		return false;
	}
	// Update positions - set[chunk_pos_in_set] should become the last element
	Set &set = m_sets[set_id];
	auto iter = set.begin() + chunk_pos_in_set;
	std::rotate(iter, iter + 1, set.end());
	// Fill the chunk from the found entry
	chunk = *(iter->chunk);
	return true;
}

void TerrainChunkCache::insert(const TerrainChunk &chunk)
{
	const size_t set_id = chunk.headerHash() % m_sets.size();
	Set &set = m_sets[set_id];

	size_t empty_pos_in_set = SET_SIZE;
	for (size_t i = 0; i < SET_SIZE; i++) {
		Entry &entry = set[i];
		if (!entry.chunk) {
			empty_pos_in_set = i;
			continue;
		}
		if (*entry.chunk == chunk) {
			assert(entry.chunk->version() <= chunk.version());
			// This chunk is already in the cache, but the content can be different
			if (entry.chunk->version() < chunk.version())
				// Update voxel data in cache
				*entry.chunk = chunk;
			return;
		}
	}
	if (empty_pos_in_set != SET_SIZE) {
		// There is an empty slot in the cache, just fill it
		set[empty_pos_in_set].chunk = new TerrainChunk(chunk);
		return;
	}
	// Replace the first element with our chunk
	set[0].replaceWith(chunk);
	// And update positions - make it the last element
	std::rotate(set.begin(), set.begin() + 1, set.end());
}

void TerrainChunkCache::invalidate(const TerrainChunk &chunk) noexcept
{
	auto[set_id, chunk_pos_in_set] = findSetAndIndex(chunk);

	if (chunk_pos_in_set != SET_SIZE)
		m_sets[set_id][chunk_pos_in_set].clear();
}

std::pair<size_t, size_t> TerrainChunkCache::findSetAndIndex(const TerrainChunk &chunk) const noexcept
{
	const size_t set_id = chunk.headerHash() % m_sets.size();
	const Set &set = m_sets[set_id];

	for (size_t i = 0; i < SET_SIZE; i++) {
		const auto &entry = set[i];
		if (!entry.chunk)
			continue;
		if (*entry.chunk == chunk)
			return { set_id, i };
	}
	// Not found
	return { set_id, SET_SIZE };
}

TerrainChunkCache::Entry::Entry(Entry &&other) noexcept
	: chunk(std::exchange(other.chunk, nullptr))
{
}

TerrainChunkCache::Entry &TerrainChunkCache::Entry::operator = (Entry &&other) noexcept
{
	chunk = std::exchange(other.chunk, nullptr);
	return *this;
}

TerrainChunkCache::Entry::~Entry() noexcept
{
	delete chunk;
}

void TerrainChunkCache::Entry::replaceWith(const TerrainChunk &chunk)
{
	clear();
	this->chunk = new TerrainChunk(chunk);
}

void TerrainChunkCache::Entry::clear() noexcept
{
	delete chunk;
	chunk = nullptr;
}

}
