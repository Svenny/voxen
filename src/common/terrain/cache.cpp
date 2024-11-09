#include <voxen/common/terrain/cache.hpp>

#include <voxen/util/log.hpp>

#include <algorithm>
#include <cassert>

namespace voxen::terrain
{

constexpr static size_t SET_SIZE = Config::CHUNK_CACHE_SET_SIZE;
constexpr static size_t MAX_CHUNKS = Config::CHUNK_CACHE_FULL_SIZE;
constexpr static size_t NUM_SETS = (MAX_CHUNKS + SET_SIZE - 1) / SET_SIZE;

ChunkCache::ChunkCache() : m_sets(NUM_SETS)
{
	constexpr size_t max_chunks = NUM_SETS * SET_SIZE;
	Log::debug("Creating terrain chunk cache, using {}x{} sets (up to {} chunks)", NUM_SETS, SET_SIZE, max_chunks);
}

bool ChunkCache::tryLoad(Chunk &chunk) noexcept
{
	auto [set_id, chunk_pos_in_set] = findSetAndIndex(chunk.id());
	if (chunk_pos_in_set == SET_SIZE) {
		// Not found
		return false;
	}

	auto &entry = m_sets[set_id][chunk_pos_in_set];
	// Move pointer out of the cache so it doesn't linger here
	extras::refcnt_ptr<Chunk> ptr = std::move(entry);
	chunk = std::move(*ptr);
	return true;
}

void ChunkCache::insert(extras::refcnt_ptr<Chunk> ptr) noexcept
{
	assert(ptr);

	const size_t set_id = ptr->id().hash() % m_sets.size();
	Set &set = m_sets[set_id];

	size_t empty_pos_in_set = SET_SIZE;
	for (size_t i = 0; i < SET_SIZE; i++) {
		auto &entry = set[i];
		if (!entry) {
			empty_pos_in_set = i;
			continue;
		}

		if (entry->id() == ptr->id()) {
			// This chunk is already in the cache, update the pointer
			assert(entry->version() <= ptr->version());
			entry = std::move(ptr);
			return;
		}
	}

	if (empty_pos_in_set != SET_SIZE) {
		// There is an empty slot in the cache, just fill it
		set[empty_pos_in_set] = std::move(ptr);
		return;
	}

	// Evict the first element in favor of incoming chunk
	std::swap(set[0], ptr);
	// And update positions - make it the last element
	std::rotate(set.begin(), set.begin() + 1, set.end());
}

void ChunkCache::invalidate(land::ChunkKey id) noexcept
{
	auto [set_id, chunk_pos_in_set] = findSetAndIndex(id);

	if (chunk_pos_in_set != SET_SIZE) {
		m_sets[set_id][chunk_pos_in_set].reset();
	}
}

std::pair<size_t, size_t> ChunkCache::findSetAndIndex(land::ChunkKey id) const noexcept
{
	const size_t set_id = id.hash() % m_sets.size();
	const Set &set = m_sets[set_id];

	for (size_t i = 0; i < SET_SIZE; i++) {
		const auto &entry = set[i];
		if (entry && entry->id() == id) {
			return { set_id, i };
		}
	}
	// Not found
	return { set_id, SET_SIZE };
}

} // namespace voxen::terrain
