#pragma once

#include <voxen/common/terrain/chunk.hpp>
#include <voxen/common/terrain/loader.hpp>

#include <cstdint>
#include <functional>

namespace voxen
{

struct TerrainQuadtreeNode;
struct TerrainOctreeNode;

class TerrainOctree {
public:
	TerrainOctree(TerrainLoader &loader, uint32_t num_xz_chunks, uint32_t num_y_chunks);
	TerrainOctree(const TerrainOctree &other);
	~TerrainOctree();

	void updateChunks(double x, double y, double z, TerrainLoader &loader);

	void walkActiveChunks(std::function<void(const TerrainChunk &)> visitor) const;
private:
	uint32_t m_xz_chunks, m_y_chunks;
	//TerrainQuadtreeNode *m_tree = nullptr;
	TerrainOctreeNode *m_tree = nullptr;
};

}
