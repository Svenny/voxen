#pragma once

#include <voxen/common/terrain/chunk.hpp>
#include <voxen/common/terrain/loader.hpp>
#include <voxen/common/threadpool.hpp>

#include <cstdint>
#include <functional>
#include <unordered_map>

namespace voxen
{
struct TerrainOctreeNode;
struct TerrainOctreeNodeHeader {
	int64_t base_x;
	int64_t base_y;
	int64_t base_z;
	int64_t size;

	bool operator == (const TerrainOctreeNodeHeader &other) const noexcept;
	uint64_t hash() const noexcept;
};
}

namespace impl
{
	struct WorkResult {
		voxen::TerrainOctreeNodeHeader requestHeader;
		voxen::TerrainOctreeNode* subnode;
	};
}

namespace voxen
{

class TerrainOctree {
public:
	struct SplitRequest {
		SplitRequest(const TerrainOctreeNodeHeader& header);
		SplitRequest(const SplitRequest& other) = default;
		SplitRequest(SplitRequest&& other) = default;

		TerrainOctreeNodeHeader subnodes_headers[8];
		TerrainOctreeNode* subnodes[8];
		bool canceled = false;
	};

public:
	explicit TerrainOctree(terrain::TerrainLoader &loader, uint32_t num_xz_chunks, uint32_t num_y_chunks);
	TerrainOctree(TerrainOctree &&other) noexcept;
	TerrainOctree(const TerrainOctree &other);
	TerrainOctree &operator = (TerrainOctree &&) = delete;
	TerrainOctree &operator = (const TerrainOctree &) = delete;
	~TerrainOctree() noexcept;

	// Call once per world tick
	void updateChunks(double x, double y, double z, terrain::TerrainLoader &loader);

	void walkActiveChunks(std::function<void(const terrain::Chunk &)> visitor) const;

	void asyncSplitNodeCreation(TerrainOctreeNodeHeader header, terrain::TerrainLoader &loader);

private:
	void loadPoolResults();
	void runDelaydedSplit(terrain::TerrainLoader &loader);

private:
	const uint32_t m_xz_chunks, m_y_chunks;
	TerrainOctreeNode *m_tree = nullptr;
	std::shared_ptr<ThreadPoolResultsQueue<impl::WorkResult>> m_created_pool_nodes;
	std::unordered_map<TerrainOctreeNodeHeader, SplitRequest, std::function<uint64_t(const TerrainOctreeNodeHeader&)>> m_loaded_nodes;
};



}
