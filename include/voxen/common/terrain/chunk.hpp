#pragma once

#include <cstdint>

namespace voxen
{

struct TerrainChunkCreateInfo {
	int64_t base_x;
	int64_t base_y;
	int64_t base_z;
	uint32_t scale;
};

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

	int64_t baseX() const noexcept { return m_base_x; }
	int64_t baseY() const noexcept { return m_base_y; }
	int64_t baseZ() const noexcept { return m_base_z; }
	uint32_t scale() const noexcept { return m_scale; }

	Data &data() noexcept { return m_data; }
	const Data &data() const noexcept { return m_data; }

	uint64_t headerHash() const noexcept;

	bool operator == (const TerrainChunk &other) const noexcept;
private:
	const int64_t m_base_x;
	const int64_t m_base_y;
	const int64_t m_base_z;
	const uint32_t m_scale;
	Data m_data;
};

}
