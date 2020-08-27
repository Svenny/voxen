#pragma once

#include <voxen/common/terrain/chunk.hpp>

#include <extras/dyn_array.hpp>

namespace voxen
{

class TerrainChunkCache {
public:
	static constexpr inline size_t SET_SIZE = 48;

	explicit TerrainChunkCache(size_t max_chunks);
	TerrainChunkCache(TerrainChunkCache &&) = delete;
	TerrainChunkCache(const TerrainChunkCache &) = delete;
	TerrainChunkCache &operator = (TerrainChunkCache &&) = delete;
	TerrainChunkCache &operator = (const TerrainChunkCache &) = delete;
	~TerrainChunkCache() = default;

	bool tryFill(TerrainChunk &chunk);
	void insert(const TerrainChunk &chunk);
	void invalidate(const TerrainChunk &chunk) noexcept;
private:
	struct Entry {
		TerrainChunk *chunk = nullptr;

		Entry() = default;
		Entry(Entry &&) noexcept;
		Entry(const Entry &) = delete;
		Entry &operator = (Entry &&) noexcept;
		Entry &operator = (const Entry &) = delete;
		~Entry() noexcept;

		void replaceWith(const TerrainChunk &chunk);
		void clear() noexcept;
	};

	using Set = std::array<Entry, SET_SIZE>;

	extras::dyn_array<Set> m_sets;

	std::pair<size_t, size_t> findSetAndIndex(const TerrainChunk &chunk) const noexcept;
};

}
