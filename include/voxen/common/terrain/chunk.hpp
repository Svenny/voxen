#pragma once

#include <voxen/common/terrain/surface.hpp>

#include <memory>
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
	static constexpr inline uint32_t SIZE = 33;
	static constexpr inline uint32_t CELL_COUNT = SIZE-1;

	using VoxelData = uint8_t[SIZE][SIZE][SIZE];
	using VoxelSurfaceData = double[SIZE][SIZE][SIZE];
	using VoxelGradientData = glm::vec3[SIZE][SIZE][SIZE];
	struct Data {
		VoxelData voxel_id;
		VoxelSurfaceData value_id;
		VoxelGradientData gradient_id;
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

	// This methods must used before and after editing voxel data
	bool beginEdit() noexcept;
	void endEdit() noexcept;

	Data &data() noexcept { return *m_data; };
	const Data &data() const noexcept { return *m_data; }

private:
	void increaseVersion() noexcept;
	void copyVoxelData();

private:
	const TerrainChunkHeader m_header;
	uint32_t m_version;
	std::shared_ptr<Data> m_data;
};


}
