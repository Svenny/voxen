#pragma once

#include <voxen/common/terrain/chunk.hpp>

#include <vector>

namespace voxen
{

class TerrainChunkSeamSet {
public:
	TerrainChunkSeamSet() = default;
	TerrainChunkSeamSet(TerrainChunkSeamSet &&) = default;
	TerrainChunkSeamSet(const TerrainChunkSeamSet &) = default;
	TerrainChunkSeamSet &operator = (TerrainChunkSeamSet &&) = default;
	TerrainChunkSeamSet &operator = (const TerrainChunkSeamSet &) = default;
	~TerrainChunkSeamSet() = default;

	template<int D> void addEdgeRef(const TerrainChunk *ptr) noexcept
	{
		assert(ptr);
		uintptr_t p = reinterpret_cast<uintptr_t>(ptr) | uintptr_t(COPY_STRATEGY_MASK_EDGE[D]);
		m_refs.emplace_back(p);
	}

	template<int D> void addFaceRef(const TerrainChunk *ptr) noexcept
	{
		assert(ptr);
		uintptr_t p = reinterpret_cast<uintptr_t>(ptr) | uintptr_t(COPY_STRATEGY_MASK_FACE[D]);
		m_refs.emplace_back(p);
	}

	void clear() noexcept;

	void extendOctree(TerrainChunkHeader header, terrain::ChunkOctree &output);

private:
	// Three least significant bits of these pointers are
	// set to the node copy strategy mask denoting which
	// dimensions must be equal to the contact point
	std::vector<uintptr_t> m_refs;

	std::tuple<uint32_t, int64_t, int64_t, int64_t> selectExtendedRootDimensions(const TerrainChunkHeader &header) const;

	static constexpr inline uint8_t COPY_STRATEGY_MASK_EDGE[3] = { 0b110, 0b101, 0b011 };
	static constexpr inline uint8_t COPY_STRATEGY_MASK_FACE[3] = { 0b001, 0b010, 0b100 };
};

}
