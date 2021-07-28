#pragma once

#include <voxen/common/terrain/chunk_id.hpp>
#include <voxen/common/terrain/config.hpp>

#include <extras/refcnt_ptr.hpp>

namespace voxen::terrain
{

class ChunkOctree;
class ChunkOwnSurface;
class ChunkSeamSurface;
struct ChunkPrimaryData;

class Chunk final {
public:
	// Determines the amount of reusing "previous" chunk
	enum class ReuseType {
		// All components of previous chunk stay the same
		Full,
		// Primary data, octree and own surface of previous
		// chunk stay the same, a new seam surface is allocated
		NoSeam,
		// Primary data and octree of previous chunk
		// stay the same, new surfaces are allocated
		NoSurface,
		// Primary data of previous chunk stays the same,
		// new octree and new surfaces are allocated
		OnlyPrimaryData,
		// All components are allocated, previous chunk is not reused
		Nothing
	};

	struct CreationInfo final {
		// ID of the to-be-created chunk. If reusing something, must be equal to `reuse_chunk->id()`.
		ChunkId id;
		// Version is set externally. Any newly created chunk must have its version strictly greater
		// than any previous one with the same ID. Otherwise undefined caching behavior will occur.
		chunk_ver_t version;
		// Determines which components of `reuse_chunk` will be copied
		ReuseType reuse_type;
		// Pointer to "predecessor" chunk to reuse pointer to some parts of it.
		// Must not be null if `reuse_type != ReuseType::Nothing`.
		const Chunk *reuse_chunk;
	};

	explicit Chunk(CreationInfo info);
	Chunk() = delete;
	Chunk(Chunk &&) = delete;
	Chunk(const Chunk &) = delete;
	Chunk &operator = (Chunk &&) noexcept;
	Chunk &operator = (const Chunk &) = delete;
	~Chunk() = default;

	const ChunkId &id() const noexcept { return m_id; }
	chunk_ver_t version() const noexcept { return m_version; }
	chunk_ver_t seamVersion() const noexcept { return m_seam_version; }

	ChunkPrimaryData &primaryData() noexcept { return *m_primary_data; }
	const ChunkPrimaryData &primaryData() const noexcept { return *m_primary_data; }

	ChunkOctree &octree() noexcept { return *m_octree; }
	const ChunkOctree &octree() const noexcept { return *m_octree; }

	ChunkOwnSurface &ownSurface() noexcept { return *m_own_surface; }
	const ChunkOwnSurface &ownSurface() const noexcept { return *m_own_surface; }

	ChunkSeamSurface &seamSurface() noexcept { return *m_seam_surface; }
	const ChunkSeamSurface &seamSurface() const noexcept { return *m_seam_surface; }

private:
	const ChunkId m_id;
	chunk_ver_t m_version;
	chunk_ver_t m_seam_version;

	extras::refcnt_ptr<ChunkPrimaryData> m_primary_data;
	extras::refcnt_ptr<ChunkOctree> m_octree;
	extras::refcnt_ptr<ChunkOwnSurface> m_own_surface;
	extras::refcnt_ptr<ChunkSeamSurface> m_seam_surface;
};

}
