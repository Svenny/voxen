#pragma once

#include <voxen/common/terrain/chunk_id.hpp>
#include <voxen/common/terrain/chunk_octree.hpp>
#include <voxen/common/terrain/config.hpp>
#include <voxen/common/terrain/primary_data.hpp>
#include <voxen/common/terrain/surface.hpp>

namespace voxen::terrain
{

class Chunk final {
public:
	struct CreationInfo final {
		// ID of the chunk to be created
		ChunkId id;
		// Version is set externally. Any newly created chunk must have its version strictly greater
		// than any previous one with the same ID. Otherwise undefined caching behavior will occur.
		chunk_ver_t version;
	};

	explicit Chunk(CreationInfo info);
	Chunk() = delete;
	Chunk(Chunk &&) = delete;
	Chunk(const Chunk &) = delete;
	Chunk &operator = (Chunk &&) noexcept;
	Chunk &operator = (const Chunk &) = delete;
	~Chunk() = default;

	// Returns `true` if this chunk does not cross the terrain surface.
	// NOTE: this method checks octree, not surface meshes. It's possible
	// and valid to return `true` even when meshes contain no indices.
	// This method can be used during (and after) contouring, right after the octree is built.
	bool hasSurface() const noexcept;
	// Returns `true` if surface does not contain any indices.
	// This condition is stricter than `hasSurface()` - this means it's
	// impossible to have `hasSurface() == false && hasSurfaceStrict() == true`.
	// This method is intended to be used on fully constructed chunks (i.e. not during contouring).
	bool hasSurfaceStrict() const noexcept;

	const ChunkId &id() const noexcept { return m_id; }
	chunk_ver_t version() const noexcept { return m_version; }

	ChunkPrimaryData &primaryData() noexcept { return m_primary_data; }
	const ChunkPrimaryData &primaryData() const noexcept { return m_primary_data; }

	ChunkOctree &octree() noexcept { return m_octree; }
	const ChunkOctree &octree() const noexcept { return m_octree; }

	ChunkSurface &surface() noexcept { return m_surface; }
	const ChunkSurface &surface() const noexcept { return m_surface; }

private:
	const ChunkId m_id;
	chunk_ver_t m_version;

	ChunkPrimaryData m_primary_data;
	ChunkOctree m_octree;
	ChunkSurface m_surface;
};

}
