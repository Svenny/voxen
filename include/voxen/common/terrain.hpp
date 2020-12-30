#pragma once

#include <voxen/common/terrain/chunk.hpp>
#include <voxen/common/terrain/loader.hpp>

#include <cstdint>
#include <functional>

namespace voxen
{

struct TerrainOctreeNode;

class TerrainOctree {
public:
	explicit TerrainOctree(TerrainLoader &loader, uint32_t num_xz_chunks, uint32_t num_y_chunks);
	TerrainOctree(TerrainOctree &&other) noexcept;
	TerrainOctree(const TerrainOctree &other);
	TerrainOctree &operator = (TerrainOctree &&) = delete;
	TerrainOctree &operator = (const TerrainOctree &) = delete;
	~TerrainOctree() noexcept;

	// Call once per world tick
	void updateChunks(double x, double y, double z, TerrainLoader &loader);

	void walkActiveChunks(std::function<void(const TerrainChunk &)> visitor) const;

private:
	const uint32_t m_xz_chunks, m_y_chunks;
	TerrainOctreeNode *m_tree = nullptr;
};

}
