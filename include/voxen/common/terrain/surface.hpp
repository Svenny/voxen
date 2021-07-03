#pragma once

#include <voxen/util/aabb.hpp>
#include <voxen/util/allocator.hpp>

#include <extras/refcnt_ptr.hpp>

#include <glm/vec3.hpp>

#include <vector>

namespace voxen::terrain
{

struct SurfaceVertex final {
	// Position in chunk-local coordinates
	glm::vec3 position;
	// Need not be normalized (despite its name lol)
	glm::vec3 normal;
};

// A base class for `ChunkOwnSurface` and `ChunkSeamSurface` for
// reusing common code. Not intended to be used on its own.
class ChunkSurfaceBase {
public:
	// Index array size is guaranteed to never exceed UINT32_MAX
	uint32_t numIndices() const noexcept { return static_cast<uint32_t>(m_indices.size()); }
	const uint32_t *indices() const noexcept { return m_indices.data(); }

	// NOTE: AABB is calculated in mesh-local coordinates and is calculated
	// by vertices, not triangles (so unused vertices also contribute to AABB)
	const Aabb &aabb() const noexcept { return m_aabb; }

protected:
	std::vector<SurfaceVertex, DomainAllocator<SurfaceVertex, AllocationDomain::TerrainMesh>> m_vertices;
	std::vector<uint32_t, DomainAllocator<uint32_t, AllocationDomain::TerrainMesh>> m_indices;
	Aabb m_aabb;

	// This class is not intended for direct use, so ctors are not public
	ChunkSurfaceBase() = default;
	ChunkSurfaceBase(ChunkSurfaceBase &&) = default;
	ChunkSurfaceBase(const ChunkSurfaceBase &) = default;
	ChunkSurfaceBase &operator = (ChunkSurfaceBase &&) = default;
	ChunkSurfaceBase &operator = (const ChunkSurfaceBase &) = default;
	~ChunkSurfaceBase() = default;

	void doClear() noexcept;
	uint32_t doAddVertex(const SurfaceVertex &vertex);
	void doAddTriangle(uint32_t a, uint32_t b, uint32_t c);
};

class ChunkOwnSurface final : public ChunkSurfaceBase {
public:
	ChunkOwnSurface() = default;
	ChunkOwnSurface(ChunkOwnSurface &&) = default;
	ChunkOwnSurface(const ChunkOwnSurface &) = default;
	ChunkOwnSurface &operator = (ChunkOwnSurface &&) = default;
	ChunkOwnSurface &operator = (const ChunkOwnSurface &) = default;
	~ChunkOwnSurface() = default;

	// Remove all added vertices and indices and reset AABB
	void clear() noexcept { doClear(); }
	// Add an entry to the end of vertex array and return its index
	uint32_t addVertex(const SurfaceVertex &vertex) { return doAddVertex(vertex); }
	// Add three indices making a triangle to the end of index array
	void addTriangle(uint32_t a, uint32_t b, uint32_t c);

	// Vertex array size is guaranteed to never exceed UINT32_MAX
	uint32_t numVertices() const noexcept { return static_cast<uint32_t>(m_vertices.size()); }
	const SurfaceVertex *vertices() const noexcept { return m_vertices.data(); }
};

class ChunkSeamSurface final : public ChunkSurfaceBase {
public:
	ChunkSeamSurface() = default;
	ChunkSeamSurface(ChunkSeamSurface &&) = default;
	ChunkSeamSurface(const ChunkSeamSurface &) = default;
	ChunkSeamSurface &operator = (ChunkSeamSurface &&) = default;
	ChunkSeamSurface &operator = (const ChunkSeamSurface &) = default;
	~ChunkSeamSurface() = default;

	// Set pointer to the "base (own) surface" for this seam surface. This operation acquires a reference.
	// NOTE: vertices and indices are not cleared, use `clear()` before `init()` if needed.
	// WARNING: base surface must always stay immutable until it's changed, index counting relies on it.
	void init(extras::refcnt_ptr<ChunkOwnSurface> base_surface) noexcept;
	// Remove all vertices, indices and reset AABB and base surface pointer
	void clear() noexcept;

	// Add an entry to the end of vertex array and return its index.
	// NOTE: indexing is including vertices from base surface.
	uint32_t addExtraVertex(const SurfaceVertex &vertex);
	// Add three indices making a triangle to the end of index array.
	// NOTE: indexing is including vertices from base surface.
	void addTriangle(uint32_t a, uint32_t b, uint32_t c);

	// Returns the sum of numbers of vertices in own and seam surface
	uint32_t numAllVertices() const noexcept;

	// Vertex array size is guaranteed to never exceed UINT32_MAX
	uint32_t numExtraVertices() const noexcept { return static_cast<uint32_t>(m_vertices.size()); }
	const SurfaceVertex *extraVertices() const noexcept { return m_vertices.data(); }

private:
	extras::refcnt_ptr<ChunkOwnSurface> m_base_surface;
};

}
