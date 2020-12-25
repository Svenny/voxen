#pragma once

#include <voxen/common/terrain/chunk_data.hpp>

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
	static constexpr inline uint32_t SIZE = TerrainChunkPrimaryData::GRID_CELL_COUNT;

	explicit TerrainChunk(const TerrainChunkCreateInfo &info);
	TerrainChunk(TerrainChunk &&) noexcept;
	TerrainChunk(const TerrainChunk &);
	TerrainChunk &operator = (TerrainChunk &&) noexcept;
	TerrainChunk &operator = (const TerrainChunk &);
	~TerrainChunk() noexcept;

	const TerrainChunkHeader& header() const noexcept { return m_header; }
	uint32_t version() const noexcept { return m_version; }

	// This methods must used before and after editing voxel data
	void beginEdit();
	void endEdit() noexcept;

	void increaseVersion() noexcept;
	void copyVoxelData();

	glm::dvec3 worldToLocal(double x, double y, double z) const noexcept;
	glm::dvec3 localToWorld(double x, double y, double z) const noexcept;

	TerrainChunkPrimaryData &primaryData() noexcept { return *m_primary_data; }
	const TerrainChunkPrimaryData &primaryData() const noexcept { return *m_primary_data; }

	TerrainChunkSecondaryData &secondaryData() noexcept { return *m_secondary_data; }
	const TerrainChunkSecondaryData &secondaryData() const noexcept { return *m_secondary_data; }

private:
	const TerrainChunkHeader m_header;
	// Assumed to be strictly increased after each change to chunk contents.
	// Yes, this means all logic will break completely when it gets past UINT32_MAX
	// (thus wrapping to zero), but we don't expect any real-world runtime to ever
	// reach values that large (it's more than 4 billion edits of a single chunk after all).
	uint32_t m_version;
	std::shared_ptr<TerrainChunkPrimaryData> m_primary_data;
	std::shared_ptr<TerrainChunkSecondaryData> m_secondary_data;
};

struct TerrainChunkEditBlock {
	TerrainChunkEditBlock(const TerrainChunkEditBlock&) = delete;
	TerrainChunkEditBlock(TerrainChunkEditBlock&&) = delete;

	TerrainChunkEditBlock(TerrainChunk& edited_chunk): chunk(edited_chunk) {
		chunk.beginEdit();
	}

	~TerrainChunkEditBlock() noexcept {
		chunk.endEdit();
	}

	TerrainChunk& chunk;
};

}
