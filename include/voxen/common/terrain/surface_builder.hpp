#pragma once

#include <voxen/common/terrain/chunk.hpp>

#include <unordered_map>

namespace voxen::terrain
{

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

	void buildFaceSeamX(const Chunk &other);
	void buildFaceSeamY(const Chunk &other);
	void buildFaceSeamZ(const Chunk &other);

	void buildEdgeSeamX(const Chunk &other_y, const Chunk &other_z, const Chunk &other_yz);
	void buildEdgeSeamY(const Chunk &other_x, const Chunk &other_z, const Chunk &other_xz);
	void buildEdgeSeamZ(const Chunk &other_x, const Chunk &other_y, const Chunk &other_xy);

private:
	Chunk &m_chunk;
	std::unordered_map<const Chunk *, uint32_t> m_foreign_chunk_to_idx;
};

}
