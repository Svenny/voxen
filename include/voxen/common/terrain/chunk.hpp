#pragma once

#include <cstdint>

namespace voxen
{

struct TerrainChunkHeader {
	int64_t base_x;
	int64_t base_y;
	int64_t base_z;
	uint32_t scale;
	bool operator == (const TerrainChunkHeader &other) const noexcept;
};

using TerrainChunkCreateInfo = TerrainChunkHeader;

class TerrainChunk {
public:
	static constexpr inline uint32_t SIZE = 32;

	struct Data {
		uint8_t voxel_id[SIZE + 1][SIZE + 1][SIZE + 1];
	};

	explicit TerrainChunk(const TerrainChunkCreateInfo &info);
	TerrainChunk(TerrainChunk &&) noexcept;
	TerrainChunk(const TerrainChunk &);
	TerrainChunk &operator = (TerrainChunk &&) noexcept;
	TerrainChunk &operator = (const TerrainChunk &);
	~TerrainChunk() noexcept;

	const TerrainChunkHeader& header() const noexcept;
	uint16_t version() const noexcept;
	void increaseVersion() noexcept;

	Data &data() noexcept { return m_data; }
	const Data &data() const noexcept { return m_data; }

	uint64_t headerHash() const noexcept;

	bool operator == (const TerrainChunk &other) const noexcept;
private:
	const TerrainChunkHeader m_header;
	uint32_t m_version;
	Data m_data;
};


}
