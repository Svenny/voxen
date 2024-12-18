#include <voxen/common/terrain/surface.hpp>

#include <algorithm>
#include <cassert>

namespace voxen::terrain
{

void ChunkSurface::clear() noexcept
{
	m_vertices.clear();
	m_indices.clear();
	m_aabb = Aabb();
}

uint32_t ChunkSurface::addVertex(const SurfaceVertex &vertex)
{
	m_vertices.emplace_back(vertex);
	m_aabb.includePoint(vertex.position);
	return static_cast<uint32_t>(m_vertices.size() - 1);
}

void ChunkSurface::addTriangle(uint32_t a, uint32_t b, uint32_t c)
{
	assert(std::max({ a, b, c }) < numVertices());

	if (a == b || b == c || a == c) {
		// Skip degenerate triangles
		return;
	}

	voxel_t m1 = m_vertices[a].materials[0];
	voxel_t m2 = m_vertices[b].materials[0];
	voxel_t m3 = m_vertices[c].materials[0];

	if (m1 == m2 && m2 == m3) {
		m_indices.emplace_back(a);
		m_indices.emplace_back(b);
		m_indices.emplace_back(c);
		return;
	}

	SurfaceVertex v1 = m_vertices[a];
	SurfaceVertex v2 = m_vertices[b];
	SurfaceVertex v3 = m_vertices[c];

	v1.materials[0] = v2.materials[0] = v3.materials[0] = m1;
	v1.materials[1] = v2.materials[1] = v3.materials[1] = m2;
	v1.materials[2] = v2.materials[2] = v3.materials[2] = m3;

	v1.flags |= 0b000;
	v2.flags |= 0b010;
	v3.flags |= 0b100;

	a = addVertex(v1);
	b = addVertex(v2);
	c = addVertex(v3);

	m_indices.emplace_back(a);
	m_indices.emplace_back(b);
	m_indices.emplace_back(c);
}

} // namespace voxen::terrain
