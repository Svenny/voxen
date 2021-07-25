#pragma once

#include <unordered_map>

#include <cstdint>

namespace voxen::terrain
{

class Chunk;
struct ChunkOctreeLeaf;

class SurfaceBuilder final {
public:
	SurfaceBuilder() = default;
	SurfaceBuilder(SurfaceBuilder &&) = delete;
	SurfaceBuilder(const SurfaceBuilder &) = delete;
	SurfaceBuilder &operator = (SurfaceBuilder &&) = delete;
	SurfaceBuilder &operator = (const SurfaceBuilder &) = delete;
	~SurfaceBuilder() = default;

	static void buildOctree(Chunk &chunk);
	static void buildOwnSurface(Chunk &chunk);

	template<int D>
	void buildFaceSeam(Chunk &me, const Chunk &other);

	template<int D>
	void buildEdgeSeam(Chunk &me, const Chunk &other_a, const Chunk &other_ab, const Chunk &other_b);

	// Reset internal vertex deduplication cache. After calling this function it's
	// valid to call `buildFaceSeam`/`buildEdgeSeam` with different value of `me`.
	void reset() noexcept;

private:
	std::unordered_map<const ChunkOctreeLeaf *, uint32_t> m_foreign_leaf_to_idx;
};

}
