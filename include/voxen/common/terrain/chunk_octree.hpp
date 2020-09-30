#pragma once

#include <voxen/common/terrain/qef_solver.hpp>
#include <voxen/common/terrain/types.hpp>

#include <array>
#include <vector>

namespace voxen
{

struct TerrainChunkOctreeLeaf {
	bool is_leaf;
	uint8_t depth;

	glm::vec3 surface_vertex;
	glm::vec3 surface_normal;
	uint32_t surface_vertex_id;
	std::array<voxel_t, 8> corners;
	QefSolver3D::State qef_state;
};
static_assert(sizeof(TerrainChunkOctreeLeaf) == 96, "96-byte TerrainChunkOctreeLeaf packing is broken");

struct TerrainChunkOctreeNode {
	bool is_leaf;
	uint8_t depth;

	uint8_t children_leafmask;
	uint32_t children_ids[8];

	bool isChildLeaf(int num) const noexcept { return (children_leafmask & (1 << num)) != 0; }
};
static_assert(sizeof(TerrainChunkOctreeNode) == 36, "36-byte TerrainChunkOctreeNode packing is broken");

static_assert(offsetof(TerrainChunkOctreeLeaf, is_leaf) == offsetof(TerrainChunkOctreeNode, is_leaf),
              "Dynamic TerrainChunkOctree[Leaf|Node] typing is broken");

class TerrainChunkOctree {
public:
	TerrainChunkOctree();
	TerrainChunkOctree(TerrainChunkOctree &&) = default;
	TerrainChunkOctree(const TerrainChunkOctree &) = default;
	TerrainChunkOctree &operator = (TerrainChunkOctree &&) = default;
	TerrainChunkOctree &operator = (const TerrainChunkOctree &) = default;
	~TerrainChunkOctree() = default;

	void generateFullTree(uint8_t depth);
	void clear() noexcept;

	TerrainChunkOctreeNode *subdivideLeaf(TerrainChunkOctreeLeaf *leaf);

	bool isRootLeaf() const noexcept;
	void *rootPointer() noexcept { return m_root_ptr; }
	const void *rootPointer() const noexcept { return m_root_ptr; }

private:
	std::vector<TerrainChunkOctreeNode> m_nodes;
	std::vector<TerrainChunkOctreeLeaf> m_leaves;

	std::vector<uint32_t> m_free_nodes;
	std::vector<uint32_t> m_free_leaves;

	// Using `void *` because it may be either node or leaf
	void *m_root_ptr = nullptr;

	std::pair<uint32_t, TerrainChunkOctreeNode *> allocNode(uint8_t depth);
	void freeNode(uint32_t idx);
	std::pair<uint32_t, TerrainChunkOctreeLeaf *> allocLeaf(uint8_t depth);
	void freeLeaf(uint32_t idx);

	void generateFullSubtree(TerrainChunkOctreeNode *node, uint8_t remaining_depth);

	void assertIsNode(const TerrainChunkOctreeNode *ptr) const noexcept;
	void assertIsLeaf(const TerrainChunkOctreeLeaf *ptr) const noexcept;
	void assertIsNodeOrLeaf(const void *ptr) const noexcept;
};

}
