#include <voxen/common/terrain/chunk.hpp>

#include <cassert>

namespace voxen::terrain
{

Chunk::Chunk(CreationInfo info) : m_id(info.id), m_version(info.version)
{}

Chunk &Chunk::operator = (Chunk &&other) noexcept
{
	assert(m_id == other.m_id);

	m_version = other.m_version;

	std::swap(m_primary_data, other.m_primary_data);
	std::swap(m_octree, other.m_octree);
	std::swap(m_surface, other.m_surface);

	return *this;
}

bool Chunk::hasSurface() const noexcept
{
	// If no octree root then surface can't contain any vertices
	return m_octree.root() != ChunkOctree::INVALID_NODE_ID;
}

bool Chunk::hasSurfaceStrict() const noexcept
{
	bool result = m_surface.numIndices() != 0;
	// `hasSurface() == false && hasSurfaceStrict() == true` is impossible
	assert(hasSurface() || !result);
	return result;
}

}
