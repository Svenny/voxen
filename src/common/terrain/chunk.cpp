#include <voxen/common/terrain/chunk.hpp>

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
	: m_header(info), m_version(0U), m_data(new Data())
{
}

TerrainChunk::TerrainChunk(TerrainChunk &&other) noexcept
	: m_header(other.m_header), m_version(other.m_version), m_data(std::move(other.m_data))
{
}

TerrainChunk::TerrainChunk(const TerrainChunk &other)
	: m_header(other.m_header), m_version(other.m_version), m_data(other.m_data)
{
}

TerrainChunk &TerrainChunk::operator = (TerrainChunk &&other) noexcept
{
	assert(m_header == other.m_header);
	assert(m_version <= other.m_version);
	if (m_version == other.m_version)
		assert(m_data == other.m_data);
	m_data = std::move(other.m_data);
	m_version = other.m_version;
	return *this;
}

TerrainChunk &TerrainChunk::operator = (const TerrainChunk &other)
{
	assert(m_header == other.m_header);
	assert(m_version <= other.m_version);
	if (m_version == other.m_version)
		assert(m_data == other.m_data);
	m_data = other.m_data;
	m_version = other.m_version;
	return *this;
}

TerrainChunk::~TerrainChunk() noexcept {
}

void TerrainChunk::copyVoxelData() {
	// Don't copy, if only this chunk uses this voxel data
	assert(m_data.use_count() != 0);
	if (m_data.use_count() != 1)
		m_data = std::shared_ptr<Data>(new Data(*m_data));
}

void TerrainChunk::beginEdit() {
	copyVoxelData();
}

void TerrainChunk::endEdit() noexcept {
	increaseVersion();
}

void TerrainChunk::increaseVersion() noexcept {
	assert(m_version != std::numeric_limits<uint32_t>::max());
	m_version++;
}

}
