#pragma once

#include <voxen/common/terrain/chunk.hpp>

#include <cassert>
#include <vector>

namespace voxen
{

// TODO: this class is deprecated and must be removed
class TerrainChunkSeamSet {
public:
	TerrainChunkSeamSet() = default;
	TerrainChunkSeamSet(TerrainChunkSeamSet &&) = default;
	TerrainChunkSeamSet(const TerrainChunkSeamSet &) = default;
	TerrainChunkSeamSet &operator = (TerrainChunkSeamSet &&) = default;
	TerrainChunkSeamSet &operator = (const TerrainChunkSeamSet &) = default;
	~TerrainChunkSeamSet() = default;

	template<int D> void addEdgeRef(const terrain::Chunk *ptr) noexcept
	{
		assert(ptr);
		uintptr_t p = reinterpret_cast<uintptr_t>(ptr) | uintptr_t(COPY_STRATEGY_MASK_EDGE[D]);
		m_refs.emplace_back(p);
	}

	template<int D> void addFaceRef(const terrain::Chunk *ptr) noexcept
	{
		assert(ptr);
		uintptr_t p = reinterpret_cast<uintptr_t>(ptr) | uintptr_t(COPY_STRATEGY_MASK_FACE[D]);
		m_refs.emplace_back(p);
	}

	void clear() noexcept;

	void extendOctree(terrain::ChunkId id, terrain::ChunkOctree &output);

private:
	// Three least significant bits of these pointers are
	// set to the node copy strategy mask denoting which
	// dimensions must be equal to the contact point
	std::vector<uintptr_t> m_refs;

	terrain::ChunkId selectExtendedRoot(terrain::ChunkId id) const;

	static constexpr inline uint8_t COPY_STRATEGY_MASK_EDGE[3] = { 0b110, 0b101, 0b011 };
	static constexpr inline uint8_t COPY_STRATEGY_MASK_FACE[3] = { 0b001, 0b010, 0b100 };
};

}
