#include <voxen/common/terrain/chunk.hpp>

#include <algorithm>
#include <cstring>

namespace voxen
{

TerrainChunk::TerrainChunk(const TerrainChunkCreateInfo &info)
	: m_base_x(info.base_x), m_base_y(info.base_y), m_base_z(info.base_z), m_scale(info.scale)
{
}

TerrainChunk::TerrainChunk(TerrainChunk &&other) noexcept
	: m_base_x(other.m_base_x), m_base_y(other.m_base_y), m_base_z(other.m_base_z),
     m_scale(other.m_scale), m_data(other.m_data)
{
}

TerrainChunk::TerrainChunk(const TerrainChunk &other)
	: m_base_x(other.m_base_x), m_base_y(other.m_base_y), m_base_z(other.m_base_z),
     m_scale(other.m_scale), m_data(other.m_data)
{
}

TerrainChunk &TerrainChunk::operator = (TerrainChunk &&other) noexcept
{
	m_data = other.m_data;
	return *this;
}

TerrainChunk &TerrainChunk::operator = (const TerrainChunk &other)
{
	m_data = other.m_data;
	return *this;
}

TerrainChunk::~TerrainChunk() noexcept
{
}

uint64_t TerrainChunk::headerHash() const noexcept
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

	conv.data.u64[0] = static_cast<uint64_t>(m_base_x);
	conv.data.u64[1] = static_cast<uint64_t>(m_base_y);
	conv.data.u64[2] = static_cast<uint64_t>(m_base_z);
	conv.data.u32 = (m_scale);
	// FNV-1a
	uint64_t result = 0xCBF29CE484222325;
	for (size_t i = 0; i < std::size(conv.bytes); i++) {
		result *= 0x100000001B3;
		result ^= uint64_t(conv.bytes[i]);
	}
	return result;
}

bool TerrainChunk::operator == (const TerrainChunk &other) const noexcept
{
	return baseX() == other.baseX() && baseY() == other.baseY() && baseZ() == other.baseZ() &&
	       scale() == other.scale();
}

}
