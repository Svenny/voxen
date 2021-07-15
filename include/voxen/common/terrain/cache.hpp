#pragma once

#include <voxen/common/terrain/chunk.hpp>
#include <voxen/common/terrain/config.hpp>
#include <voxen/util/allocator.hpp>

#include <extras/dyn_array.hpp>

namespace voxen::terrain
{

class ChunkCache final {
public:
	ChunkCache();
	ChunkCache(ChunkCache &&) = delete;
	ChunkCache(const ChunkCache &) = delete;
	ChunkCache &operator = (ChunkCache &&) = delete;
	ChunkCache &operator = (const ChunkCache &) = delete;
	~ChunkCache() = default;

	// Lookup chunk with given ID in the cache.
	// Returns null pointer if there is no such chunk.
	// On cache hit chunk is automatically evicted from the cache, as otherwise it
	// will be never destroyed (cache holds a reference) until evicted by some `insert`.
	extras::refcnt_ptr<Chunk> tryLoad(ChunkId id) noexcept;
	// Insert a chunk into the cache. Updates the pointer
	// if chunk with the same ID is already in the cache.
	void insert(extras::refcnt_ptr<Chunk> ptr) noexcept;
	// Remove chunk with given ID from the cache (if present)
	void invalidate(ChunkId id) noexcept;

private:
	// The cache does not have its own storage, but it does merely reuse chunk pool.
	// This increases common pool pressure, but allows zero-copy load on cache hit.
	using Set = std::array<extras::refcnt_ptr<Chunk>, Config::CHUNK_CACHE_SET_SIZE>;

	extras::dyn_array<Set, DomainAllocator<Set, AllocationDomain::StandbyCache>> m_sets;

	std::pair<size_t, size_t> findSetAndIndex(ChunkId id) const noexcept;
};

}
