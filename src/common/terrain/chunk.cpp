#include <voxen/common/terrain/chunk.hpp>

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
	union {
		struct {
			uint64_t u64[3];
			uint32_t u32;
		} data;
		uint8_t bytes[sizeof(data)];
	} conv;
#pragma pack(pop)

	conv.data.u64[0] = static_cast<uint64_t>(base_x);
	conv.data.u64[1] = static_cast<uint64_t>(base_y);
	conv.data.u64[2] = static_cast<uint64_t>(base_z);
	conv.data.u32 = (scale);
	// FNV-1a
	uint64_t result = 0xCBF29CE484222325;
	for (size_t i = 0; i < std::size(conv.bytes); i++) {
		result *= 0x100000001B3;
		result ^= uint64_t(conv.bytes[i]);
	}
	return result;
}

TerrainChunk::TerrainChunk(const TerrainChunkCreateInfo &info)
	: m_header(info), m_version(0U)
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
	m_data = std::move(other.m_data);
	return *this;
}

TerrainChunk &TerrainChunk::operator = (const TerrainChunk &other)
{
	assert(m_header == other.m_header);
	m_data = other.m_data;
	return *this;
}

TerrainChunk::~TerrainChunk() noexcept
{
}

void TerrainChunk::increaseVersion() noexcept
{
	assert(m_version != std::numeric_limits<uint32_t>::max());
	m_version++;
}

}
