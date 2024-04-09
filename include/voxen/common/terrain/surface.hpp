#pragma once

#include <voxen/common/terrain/config.hpp>
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
	// Surface normal (unit vector)
	glm::vec3 normal;
	// Primary surface material (the only one in single-material parts)
	voxel_t primary_mat;
	// Properties of this vertex:
	// - bit 0: 'is joint/flange vertex' flag
	uint8_t flags;
	// Weight of secondary materials pair, normalized value from 0.0 to 1.0.
	// If 0 then this vertex belongs to a single-material part.
	uint8_t secondary_mats_weight;
	// Ratio of secondary materials pair, normalized value from 0.0 to 1.0.
	// 0.0 is 100% `secondary_mat_a`, 1.0 is 100% `secondary_mat_b`, 0.5 is 50% both.
	uint8_t secondary_mats_ratio;
	// Secondary material A (ingored if `secondary_mats_weight == 0 || secondary_mats_ratio == 255`)
	voxel_t secondary_mat_a;
	// Secondary material B (ignored if `secondary_mats_weight == 0 || secondary_mats_ratio == 0`)
	voxel_t secondary_mat_b;
	// Unused padding bits
	uint16_t reserved;
};
static_assert(sizeof(SurfaceVertex) == 32, "32-byte SurfaceVertex packing is broken");

class ChunkSurface final {
public:
	ChunkSurface() = default;
	ChunkSurface(ChunkSurface &&) = default;
	ChunkSurface(const ChunkSurface &) = default;
	ChunkSurface &operator=(ChunkSurface &&) = default;
	ChunkSurface &operator=(const ChunkSurface &) = default;
	~ChunkSurface() = default;

	// Remove all added vertices and indices and reset AABB
	void clear() noexcept;
	// Add an entry to the end of vertex array and return its index
	uint32_t addVertex(const SurfaceVertex &vertex);
	// Add three indices making a triangle to the end of index array
	void addTriangle(uint32_t a, uint32_t b, uint32_t c);

	// Vertex array size is guaranteed to never exceed UINT32_MAX
	uint32_t numVertices() const noexcept { return static_cast<uint32_t>(m_vertices.size()); }
	const SurfaceVertex *vertices() const noexcept { return m_vertices.data(); }

	// Index array size is guaranteed to never exceed UINT32_MAX
	uint32_t numIndices() const noexcept { return static_cast<uint32_t>(m_indices.size()); }
	const uint32_t *indices() const noexcept { return m_indices.data(); }

	// NOTE: AABB is calculated in mesh-local coordinates and is calculated
	// by vertices, not triangles (so unused vertices also contribute to AABB)
	const Aabb &aabb() const noexcept { return m_aabb; }

private:
	std::vector<SurfaceVertex, DomainAllocator<SurfaceVertex, AllocationDomain::TerrainMesh>> m_vertices;
	std::vector<uint32_t, DomainAllocator<uint32_t, AllocationDomain::TerrainMesh>> m_indices;
	Aabb m_aabb;
};

} // namespace voxen::terrain
