#include <voxen/common/terrain/surface.hpp>

#include <cassert>
#include <limits>

namespace voxen
{

TerrainSurface::TerrainSurface(size_t reserve_vertices, size_t reserve_indices)
{
	m_vertices.reserve(reserve_vertices);
	m_indices.reserve(reserve_indices);
}

uint32_t TerrainSurface::addVertex(const TerrainSurfaceVertex &vertex)
{
	assert(m_vertices.size() <= std::numeric_limits<uint32_t>::max());
	m_vertices.emplace_back(vertex);
	return static_cast<uint32_t>(m_vertices.size() - 1);
}

void TerrainSurface::addTriangle(uint32_t a, uint32_t b, uint32_t c)
{
	assert(std::max({ a, b, c }) < m_vertices.size());
	m_indices.emplace_back(a);
	m_indices.emplace_back(b);
	m_indices.emplace_back(c);
}

}
