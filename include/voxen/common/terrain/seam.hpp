#pragma once

#include <voxen/common/terrain/chunk.hpp>
#include <voxen/common/terrain/types.hpp>

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

	void addEdgeRef(const TerrainChunk *ptr, int dim) noexcept
	{
		assert(ptr);
		m_edge_refs[dim].emplace_back(ptr, ptr->version());
	}

	void addFaceRef(const TerrainChunk *ptr, int dim) noexcept
	{
		assert(ptr);
		m_face_refs[dim].emplace_back(ptr, ptr->version());
	}

	void clear() noexcept;

	void extendOctree(TerrainChunkHeader header, ChunkOctree &output);

	bool operator == (const TerrainChunkSeamSet &other) const noexcept;

private:
	using ChunkRef = std::pair<const TerrainChunk *, uint32_t>;

	std::vector<ChunkRef> m_edge_refs[3];
	std::vector<ChunkRef> m_face_refs[3];

	std::tuple<uint32_t, int64_t, int64_t, int64_t> selectExtendedRootDimensions(const TerrainChunkHeader &header) const;
};

}
