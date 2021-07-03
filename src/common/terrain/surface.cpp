#include <voxen/common/terrain/surface.hpp>

#include <cassert>
#include <limits>

namespace voxen::terrain
{

void ChunkSurfaceBase::doClear() noexcept
{
	m_vertices.clear();
	m_indices.clear();
	m_aabb = Aabb();
}

uint32_t ChunkSurfaceBase::doAddVertex(const SurfaceVertex &vertex)
{
	m_vertices.emplace_back(vertex);
	m_aabb.includePoint(vertex.position);
	return static_cast<uint32_t>(m_vertices.size() - 1);
}

void ChunkSurfaceBase::doAddTriangle(uint32_t a, uint32_t b, uint32_t c)
{
	m_indices.emplace_back(a);
	m_indices.emplace_back(b);
	m_indices.emplace_back(c);
}

void ChunkOwnSurface::addTriangle(uint32_t a, uint32_t b, uint32_t c)
{
	assert(std::max({ a, b, c }) < numVertices());
	doAddTriangle(a, b, c);
}

void ChunkSeamSurface::init(extras::refcnt_ptr<ChunkOwnSurface> base_surface) noexcept
{
	assert(base_surface);
	doClear();
	m_base_surface = std::move(base_surface);
	m_aabb = m_base_surface->aabb();
}

void ChunkSeamSurface::clear() noexcept
{
	doClear();
	m_base_surface.reset();
}

uint32_t ChunkSeamSurface::addExtraVertex(const SurfaceVertex &vertex)
{
	assert(m_base_surface);
	uint32_t id = doAddVertex(vertex);
	return id + m_base_surface->numVertices();
}

void ChunkSeamSurface::addTriangle(uint32_t a, uint32_t b, uint32_t c)
{
	assert(std::max({ a, b, c }) < numAllVertices());
	doAddTriangle(a, b, c);
}

uint32_t ChunkSeamSurface::numAllVertices() const noexcept
{
	assert(m_base_surface);
	return m_base_surface->numVertices() + numExtraVertices();
}

}
