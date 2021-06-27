#pragma once

#include <voxen/common/terrain/chunk_data.hpp>
#include <voxen/common/terrain/chunk_header.hpp>

#include <memory>
#include <cstdint>

namespace voxen
{

class TerrainChunk {
public:
	// TODO: remove this deprecated alias
	static constexpr inline uint32_t SIZE = terrain::Config::CHUNK_SIZE;

	explicit TerrainChunk(const TerrainChunkHeader &header);
	TerrainChunk(TerrainChunk &&) noexcept;
	TerrainChunk(const TerrainChunk &);
	TerrainChunk &operator = (TerrainChunk &&) noexcept;
	TerrainChunk &operator = (const TerrainChunk &);
	~TerrainChunk() = default;

	const TerrainChunkHeader &header() const noexcept { return m_header; }
	uint32_t version() const noexcept { return m_version; }

	// This methods must used before and after editing voxel data
	std::pair<TerrainChunkPrimaryData &, TerrainChunkSecondaryData &> beginEdit();
	void endEdit() noexcept;

	void increaseVersion() noexcept;
	void copyVoxelData();

	glm::dvec3 worldToLocal(double x, double y, double z) const noexcept { return m_header.worldToLocal(x, y, z); }
	glm::dvec3 localToWorld(double x, double y, double z) const noexcept { return m_header.localToWorld(x, y, z); }

	// Only `const` getters. Use `beginEdit()`/`endEdit()` to obtain non-const references.
	const TerrainChunkPrimaryData &primaryData() const noexcept { return *m_primary_data; }
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
	TerrainChunkEditBlock(TerrainChunkEditBlock &&) = delete;
	TerrainChunkEditBlock(const TerrainChunkEditBlock &) = delete;
	TerrainChunkEditBlock &operator = (TerrainChunkEditBlock &&) = delete;
	TerrainChunkEditBlock &operator = (const TerrainChunkEditBlock &) = delete;

	explicit TerrainChunkEditBlock(TerrainChunk &edited_chunk) : chunk(edited_chunk)
	{
		auto[primary, secondary] = chunk.beginEdit();
		primary_data = &primary;
		secondary_data = &secondary;
	}

	~TerrainChunkEditBlock() noexcept
	{
		chunk.endEdit();
	}

	TerrainChunk &chunk;
	TerrainChunkPrimaryData *primary_data = nullptr;
	TerrainChunkSecondaryData *secondary_data = nullptr;
};

}
