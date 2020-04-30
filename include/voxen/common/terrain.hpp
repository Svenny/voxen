#pragma once

#include <functional>

#include <cstdint>

namespace voxen
{

class TerrainChunk {
public:
	TerrainChunk(uint32_t size, uint32_t scale, int64_t base_x, int64_t base_y, int64_t base_z);
	~TerrainChunk() = default;

	uint32_t size() const noexcept { return m_size; }
	uint32_t scale() const noexcept { return m_scale; }
	int64_t baseX() const noexcept { return m_base_x; }
	int64_t baseY() const noexcept { return m_base_y; }
	int64_t baseZ() const noexcept { return m_base_z; }

	bool operator==(const TerrainChunk &other) const noexcept;
private:
	uint32_t m_size;
	uint32_t m_scale;
	int64_t m_base_x, m_base_y, m_base_z;
};

struct TerrainChunkCache;

class TerrainChunkGenerator {
public:
	TerrainChunkGenerator();
	~TerrainChunkGenerator();

	void generate(TerrainChunk &chunk);
private:
	TerrainChunkCache *m_cache = nullptr;
};

struct TerrainQuadtreeNode;
struct TerrainOctreeNode;

class TerrainOctree {
public:
	TerrainOctree(uint32_t num_xz_chunks, uint32_t num_y_chunks);
	~TerrainOctree();

	void updateChunks(double x, double y, double z);

	void walk(std::function<void(int64_t, int64_t, int64_t, int64_t)> callback) const;
private:
	uint32_t m_xz_chunks, m_y_chunks;
	TerrainChunkGenerator m_chunk_gen;
	//TerrainQuadtreeNode *m_tree = nullptr;
	TerrainOctreeNode *m_tree = nullptr;
};

}
