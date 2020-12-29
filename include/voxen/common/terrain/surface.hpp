#pragma once

#include <glm/glm.hpp>

#include <vector>

namespace voxen
{

struct TerrainSurfaceVertex {
	// Position in mesh-local coordinates
	glm::vec3 position;
	// Need not be normalized (despite its name lol)
	glm::vec3 normal;
};

class TerrainSurface {
public:
	TerrainSurface() = default;
	explicit TerrainSurface(size_t reserve_vertices, size_t reserve_indices);
	TerrainSurface(TerrainSurface &&) = default;
	TerrainSurface(const TerrainSurface &) = default;
	TerrainSurface &operator = (TerrainSurface &&) = default;
	TerrainSurface &operator = (const TerrainSurface &) = default;
	~TerrainSurface() = default;

	void clear() noexcept;

	// Add an entry to the end of vertex array and return its index
	uint32_t addVertex(const TerrainSurfaceVertex &vertex);
	// Add three indices making a triangle to the end of index array
	void addTriangle(uint32_t a, uint32_t b, uint32_t c);

	// Vertex array size is guaranteed to never exceed UINT32_MAX
	uint32_t numVertices() const noexcept { return static_cast<uint32_t>(m_vertices.size()); }
	const TerrainSurfaceVertex *vertices() const noexcept { return m_vertices.data(); }
	TerrainSurfaceVertex *vertices() noexcept { return m_vertices.data(); }

	// Index array size is guaranteed to never exceed UINT32_MAX
	uint32_t numIndices() const noexcept { return static_cast<uint32_t>(m_indices.size()); }
	const uint32_t *indices() const noexcept { return m_indices.data(); }

	// NOTE: AABB is calculated in mesh-local coordinates and is calculated
	// by vertices, not triangles (so unused vertices contribute to AABB)

	// Minimal (X,Y,Z) local-space coordinates of AABB
	const glm::vec3 &aabbMin() const noexcept { return m_aabb_min; }
	// Maximal (X,Y,Z) local-space coordinates of AABB
	const glm::vec3 &aabbMax() const noexcept { return m_aabb_max; }

private:
	std::vector<TerrainSurfaceVertex> m_vertices;
	std::vector<uint32_t> m_indices;
	glm::vec3 m_aabb_min { FLT_MAX, FLT_MAX, FLT_MAX };
	glm::vec3 m_aabb_max { FLT_MIN, FLT_MIN, FLT_MIN };
};

}
