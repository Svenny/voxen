#include <voxen/common/terrain/chunk_octree.hpp>

#include <cassert>
#include <tuple>

namespace voxen
{

// Depth of 11 would require at worst (2^11)^3 = 8'589'934'592 leaves, which overflows uint32_t
static constexpr uint8_t MAX_ALLOWED_DEPTH = 10;
static constexpr uint8_t LEAFMASK_NONE = 0b00000000;
static constexpr uint8_t LEAFMASK_FULL = 0b11111111;
static constexpr uint32_t CHILD_ID_INVALID = UINT32_MAX;

void TerrainChunkOctree::generateFullTree(uint8_t max_depth)
{
	assert(max_depth <= MAX_ALLOWED_DEPTH);
	clear();

	if (max_depth == 0) {
		m_root_ptr = allocLeaf(0).second;
		return;
	}

	m_root_ptr = allocNode(0).second;
	// Root is a node, guaranteed by above line
	auto root = reinterpret_cast<TerrainChunkOctreeNode *>(m_root_ptr);
	generateFullSubtree(root, max_depth - 1);
}

void TerrainChunkOctree::clear() noexcept
{
	m_nodes.clear();
	m_leaves.clear();
	m_free_nodes.clear();
	m_free_leaves.clear();
	m_root_ptr = nullptr;
}

TerrainChunkOctreeNode *TerrainChunkOctree::subdivideLeaf(TerrainChunkOctreeLeaf *leaf)
{
	assertIsLeaf(leaf);
	assert(leaf->depth < MAX_ALLOWED_DEPTH);

	auto[new_idx, new_node] = allocNode(leaf->depth);
	// All children are leaves
	new_node->children_leafmask = LEAFMASK_FULL;
	// No-alloc copy leaf to the first child
	new_node->children_ids[0] = uint32_t(leaf - m_leaves.data());

	uint8_t child_depth = leaf->depth + 1;
	leaf->depth = child_depth;
	// Alloc 7 other children
	for (int i = 1; i < 8; i++)
		new_node->children_ids[i] = allocLeaf(child_depth).first;

	return new_node;
}

std::pair<uint32_t, TerrainChunkOctreeNode *> TerrainChunkOctree::allocNode(uint8_t depth)
{
	assert(depth <= MAX_ALLOWED_DEPTH);

	uint32_t idx;
	if (!m_free_nodes.empty()) {
		idx = m_free_nodes.back();
		m_free_nodes.pop_back();
	} else {
		idx = uint32_t(m_nodes.size());
		m_nodes.emplace_back();
	}

	TerrainChunkOctreeNode *node = &m_nodes[idx];
	node->is_leaf = false;
	node->depth = depth;
	std::fill_n(node->children_ids, 8, CHILD_ID_INVALID);
	return { idx, node };
}

bool TerrainChunkOctree::isRootLeaf() const noexcept
{
	assert(m_root_ptr != nullptr);
	const TerrainChunkOctreeNode *node = reinterpret_cast<const TerrainChunkOctreeNode *>(m_root_ptr);
	return node->is_leaf;
}

void TerrainChunkOctree::freeNode(uint32_t idx)
{
	m_free_nodes.emplace_back(idx);
}

std::pair<uint32_t, TerrainChunkOctreeLeaf *> TerrainChunkOctree::allocLeaf(uint8_t depth)
{
	assert(depth <= MAX_ALLOWED_DEPTH);

	uint32_t idx;
	if (!m_free_leaves.empty()) {
		idx = m_free_leaves.back();
		m_free_leaves.pop_back();
	} else {
		idx = uint32_t(m_leaves.size());
		m_leaves.emplace_back();
	}

	TerrainChunkOctreeLeaf *leaf = &m_leaves[idx];
	leaf->is_leaf = true;
	leaf->depth = depth;
	return { idx, leaf };
}

void TerrainChunkOctree::freeLeaf(uint32_t idx)
{
	m_free_leaves.emplace_back(idx);
}

void TerrainChunkOctree::generateFullSubtree(TerrainChunkOctreeNode *node, uint8_t remaining_depth)
{
	assertIsNode(node);
	if (remaining_depth == 0) {
		node->children_leafmask = LEAFMASK_FULL;
		for (int i = 0; i < 8; i++) {
			TerrainChunkOctreeLeaf *leaf;
			std::tie(node->children_ids[i], leaf) = allocLeaf(node->depth + 1);
		}
	} else {
		node->children_leafmask = LEAFMASK_NONE;
		for (int i = 0; i < 8; i++) {
			TerrainChunkOctreeNode *child;
			std::tie(node->children_ids[i], child) = allocNode(node->depth + 1);
			generateFullSubtree(child, remaining_depth - 1);
		}
	}
}

void TerrainChunkOctree::assertIsNode(const TerrainChunkOctreeNode *ptr) const noexcept
{
	(void)ptr; // For builds with disabled asserts
	assert(ptr >= m_nodes.data() && ptr < m_nodes.data() + m_nodes.size() && !ptr->is_leaf);
}

void TerrainChunkOctree::assertIsLeaf(const TerrainChunkOctreeLeaf *ptr) const noexcept
{
	(void)ptr; // For builds with disabled asserts
	assert(ptr >= m_leaves.data() && ptr < m_leaves.data() + m_leaves.size() && ptr->is_leaf);
}

void TerrainChunkOctree::assertIsNodeOrLeaf(const void *ptr) const noexcept
{
	(void)ptr; // For builds with disabled asserts
	assert((ptr >= m_nodes.data() && ptr < m_nodes.data() + m_nodes.size()) ||
	       (ptr >= m_leaves.data() && ptr < m_leaves.data() + m_leaves.size()));
}

}
