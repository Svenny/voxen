#pragma once

#include <voxen/common/terrain/surface.hpp>

#include <cstdint>

namespace voxen
{

struct TerrainChunkHeader {
	int64_t base_x;
	int64_t base_y;
	int64_t base_z;
	uint32_t scale;
	bool operator == (const TerrainChunkHeader &other) const noexcept;
	uint64_t hash() const noexcept;
};

using TerrainChunkCreateInfo = TerrainChunkHeader;

class TerrainChunk {
public:
	static constexpr inline uint32_t SIZE = 32;

	struct Data {
		uint8_t voxel_id[SIZE][SIZE][SIZE];
		TerrainSurface surface;
	};

	explicit TerrainChunk(const TerrainChunkCreateInfo &info);
	TerrainChunk(TerrainChunk &&) noexcept;
	TerrainChunk(const TerrainChunk &);
	TerrainChunk &operator = (TerrainChunk &&) noexcept;
	TerrainChunk &operator = (const TerrainChunk &);
	~TerrainChunk() noexcept;

	const TerrainChunkHeader& header() const noexcept { return m_header; }
	uint32_t version() const noexcept { return m_version; }
	void increaseVersion() noexcept;

	Data &data() noexcept { return m_data; }
	const Data &data() const noexcept { return m_data; }
private:
	const TerrainChunkHeader m_header;
	uint32_t m_version;
	Data m_data;
};


}
