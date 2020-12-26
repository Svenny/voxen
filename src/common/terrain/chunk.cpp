#include <voxen/common/terrain/chunk.hpp>

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
	if (m_version == other.m_version) {
		// Equal versions must guarantee data equality, so no need to transfer anything
		assert(primaryData() == other.primaryData());
		// Secondary data is not compared as it must be a function of primary data
	} else {
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
	if (m_version == other.m_version) {
		// Equal versions must guarantee data equality, so no need to transfer anything
		assert(primaryData() == other.primaryData());
		// Secondary data is not compared as it must be a function of primary data
	} else {
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
