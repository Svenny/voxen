#include <voxen/common/terrain/chunk.hpp>

#include <voxen/common/terrain/surface_builder.hpp>
#include <voxen/util/hash.hpp>
#include <voxen/util/log.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <cassert>

namespace voxen
{

bool TerrainChunkHeader::operator== (const TerrainChunkHeader &other) const noexcept {
	return base_x == other.base_x && base_y == other.base_y && base_z == other.base_z && scale == other.scale;
}

uint64_t TerrainChunkHeader::hash() const noexcept
{
#pragma pack(push, 1)
	struct {
		uint64_t u64[3];
		uint32_t u32;
	} data;
#pragma pack(pop)

	data.u64[0] = static_cast<uint64_t>(base_x);
	data.u64[1] = static_cast<uint64_t>(base_y);
	data.u64[2] = static_cast<uint64_t>(base_z);
	data.u32 = (scale);

	return hashFnv1a(&data, sizeof(data));
}

TerrainChunk::TerrainChunk(const TerrainChunkCreateInfo &info)
	: m_header(info), m_version(0U),
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

TerrainChunk::~TerrainChunk() noexcept
{
}

void TerrainChunk::beginEdit()
{
	copyVoxelData();
}

void TerrainChunk::endEdit() noexcept
{
	increaseVersion();
	// HACK: remove this
	// TODO: World should do this
	// This also violates `noexcept` guarantee
	TerrainSurfaceBuilder::calcSurface(primaryData(), secondaryData());
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

glm::dvec3 TerrainChunk::worldToLocal(double x, double y, double z) const noexcept
{
	glm::dvec3 p(x, y, z);
	p -= glm::dvec3(m_header.base_x, m_header.base_y, m_header.base_z);
	p /= double(m_header.scale);
	return p;
}

glm::dvec3 TerrainChunk::localToWorld(double x, double y, double z) const noexcept
{
	glm::dvec3 p(x, y, z);
	p *= double(m_header.scale);
	p += glm::dvec3(m_header.base_x, m_header.base_y, m_header.base_z);
	return p;
}

}
