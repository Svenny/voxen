#include <voxen/common/terrain/chunk.hpp>

#include <voxen/common/terrain/chunk_octree.hpp>
#include <voxen/common/terrain/primary_data.hpp>
#include <voxen/common/terrain/surface.hpp>

#include <cassert>

namespace voxen::terrain
{

Chunk::Chunk(CreationInfo info) noexcept : m_id(info.id)
{
	if (info.reuse_type != ReuseType::Nothing) {
		assert(info.reuse_chunk);
		// Though there is no such technical limitation, reusing
		// a chunk from different location is a logical error
		assert(info.reuse_chunk->id() == m_id);
	}

	m_primary_data = std::move(info.new_primary_data);
	m_octree = std::move(info.new_octree);
	m_own_surface = std::move(info.new_own_surface);
	m_seam_surface = std::move(info.new_seam_surface);

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
	assert(m_primary_data);
	assert(m_octree);
	assert(m_own_surface);
	assert(m_seam_surface);

	if (info.reuse_type != ReuseType::Full) {
		m_seam_surface->init(m_own_surface);
	}
}

}

#include <cassert>
#include <limits>

namespace voxen
{

TerrainChunk::TerrainChunk(const TerrainChunkHeader &header)
	: m_header(header), m_version(0U),
	m_primary_data(new TerrainChunkPrimaryData), m_secondary_data(new TerrainChunkSecondaryData)
{
}

TerrainChunk::TerrainChunk(TerrainChunk &&other) noexcept
	: m_header(other.m_header), m_version(other.m_version),
	m_primary_data(std::move(other.m_primary_data)), m_secondary_data(std::move(other.m_secondary_data))
{
}

TerrainChunk::TerrainChunk(const TerrainChunk &other)
	: m_header(other.m_header), m_version(other.m_version),
	m_primary_data(other.m_primary_data), m_secondary_data(other.m_secondary_data)
{
}

TerrainChunk &TerrainChunk::operator = (TerrainChunk &&other) noexcept
{
	assert(m_header == other.m_header);
	assert(m_version <= other.m_version);

	// Equal versions must guarantee data equality, so copy only when they differ
	if (m_version != other.m_version) {
		m_version = other.m_version;
		m_primary_data = std::move(other.m_primary_data);
		m_secondary_data = std::move(other.m_secondary_data);
	}
	return *this;
}

TerrainChunk &TerrainChunk::operator = (const TerrainChunk &other)
{
	assert(m_header == other.m_header);
	assert(m_version <= other.m_version);

	// Equal versions must guarantee data equality, so copy only when they differ
	if (m_version != other.m_version) {
		m_version = other.m_version;
		m_primary_data = other.m_primary_data;
		m_secondary_data = other.m_secondary_data;
	}
	return *this;
}

std::pair<TerrainChunkPrimaryData &, TerrainChunkSecondaryData &> TerrainChunk::beginEdit()
{
	copyVoxelData();
	return { *m_primary_data, *m_secondary_data };
}

void TerrainChunk::endEdit() noexcept
{
	increaseVersion();
}

void TerrainChunk::increaseVersion() noexcept
{
	assert(m_version != std::numeric_limits<uint32_t>::max());
	m_version++;
}

void TerrainChunk::copyVoxelData()
{
	// Don't copy, if only this chunk uses this voxel data
	assert(m_primary_data.use_count() != 0);
	if (m_primary_data.use_count() != 1) {
		m_primary_data = std::shared_ptr<TerrainChunkPrimaryData>(new TerrainChunkPrimaryData(*m_primary_data));
	}
	assert(m_secondary_data.use_count() != 0);
	if (m_secondary_data.use_count() != 1) {
		m_secondary_data = std::shared_ptr<TerrainChunkSecondaryData>(new TerrainChunkSecondaryData(*m_secondary_data));
	}
}

}
