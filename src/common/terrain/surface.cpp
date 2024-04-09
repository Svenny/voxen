#include <voxen/common/terrain/surface.hpp>

#include <cassert>
#include <limits>

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

	m_indices.emplace_back(a);
	m_indices.emplace_back(b);
	m_indices.emplace_back(c);
}

} // namespace voxen::terrain
