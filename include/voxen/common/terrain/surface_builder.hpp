#pragma once

#include <voxen/common/terrain/chunk.hpp>

#include <unordered_map>

namespace voxen::terrain
{

struct ChunkOctreeLeaf;

class SurfaceBuilder final {
public:
	explicit SurfaceBuilder(Chunk &chunk) noexcept : m_chunk(chunk) {}
	SurfaceBuilder(SurfaceBuilder &&) = delete;
	SurfaceBuilder(const SurfaceBuilder &) = delete;
	SurfaceBuilder &operator = (SurfaceBuilder &&) = delete;
	SurfaceBuilder &operator = (const SurfaceBuilder &) = delete;
	~SurfaceBuilder() = default;

	void buildOctree();
	void buildOwnSurface();

	template<int D>
	void buildFaceSeam(const Chunk &other);

	template<int D>
	void buildEdgeSeam(const Chunk &other_a, const Chunk &other_ab, const Chunk &other_b);

private:
	Chunk &m_chunk;
	std::unordered_map<const ChunkOctreeLeaf *, uint32_t> m_foreign_leaf_to_idx;
};

}
