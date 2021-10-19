#include <voxen/common/terrain/chunk.hpp>

#include <voxen/common/terrain/allocator.hpp>
#include <voxen/common/terrain/chunk_octree.hpp>
#include <voxen/common/terrain/primary_data.hpp>
#include <voxen/common/terrain/surface.hpp>

#include <cassert>

namespace voxen::terrain
{

static uint32_t calcSeamVersion(const Chunk::CreationInfo &info) noexcept
{
	const Chunk *base = info.reuse_chunk;

	if (!base) {
		// No predecessor, seam versioning begins
		return 0;
	}

	if (info.reuse_type == Chunk::ReuseType::Full) {
		// Full reuse means seam stays the same
		return info.reuse_chunk->seamVersion();
	}

	if (info.version != base->version()) {
		// Primary version changes, may reset seam versioning
		return 0;
	}

	// Primary version is the same, continue seam versioning
	return base->seamVersion() + 1;
}

Chunk::Chunk(CreationInfo info) : m_id(info.id), m_version(info.version), m_seam_version(calcSeamVersion(info))
{
	if (info.reuse_type != ReuseType::Nothing) {
		assert(info.reuse_chunk);
		// Though there is no such technical limitation, reusing
		// a chunk from different location is a logical error
		assert(info.reuse_chunk->id() == m_id);
		// Note that this check is not sufficient to catch all cases of bad version management.
		// It's possible that chunk with this ID was removed in some tick and then created again,
		// in which case there would be no `info.reuse_chunk` but version must still be increased.
		assert(info.reuse_chunk->version() <= m_version);
	}

	switch (info.reuse_type) {
	case ReuseType::Full:
		m_seam_surface = info.reuse_chunk->m_seam_surface;
		[[fallthrough]];
	case ReuseType::NoSeam:
		m_own_surface = info.reuse_chunk->m_own_surface;
		[[fallthrough]];
	case ReuseType::NoSurface:
		m_octree = info.reuse_chunk->m_octree;
		[[fallthrough]];
	case ReuseType::OnlyPrimaryData:
		m_primary_data = info.reuse_chunk->m_primary_data;
		break;
	case ReuseType::Nothing:
		// Well, nothing
		break;
	}

	// We hold invariant that all components exist after construction
	if (!m_primary_data) {
		m_primary_data = PoolAllocator::allocatePrimaryData();
	}

	if (!m_octree) {
		m_octree = PoolAllocator::allocateOctree();
	}

	if (!m_own_surface) {
		m_own_surface = PoolAllocator::allocateOwnSurface();
	}

	if (!m_seam_surface) {
		m_seam_surface = PoolAllocator::allocateSeamSurface();
		m_seam_surface->init(m_own_surface);
	}
}

Chunk &Chunk::operator = (Chunk &&other) noexcept
{
	assert(m_id == other.m_id);

	m_version = other.m_version;
	m_seam_version = other.m_seam_version;

	std::swap(m_primary_data, other.m_primary_data);
	std::swap(m_octree, other.m_octree);
	std::swap(m_own_surface, other.m_own_surface);
	std::swap(m_seam_surface, other.m_seam_surface);

	return *this;
}

bool Chunk::hasSurface() const noexcept
{
	// If no octree root then neither own nor seam surface can contain any vertices
	return octree().root() != ChunkOctree::INVALID_NODE_ID;
}

bool Chunk::hasSurfaceStrict() const noexcept
{
	bool result = m_own_surface->numIndices() != 0 || m_seam_surface->numIndices() != 0;
	// `hasSurface() == false && hasSurfaceStrict() == true` is impossible
	assert(hasSurface() || !result);
	return result;
}

}
