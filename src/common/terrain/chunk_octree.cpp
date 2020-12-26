#include <voxen/common/terrain/chunk_octree.hpp>

#include <cassert>
#include <tuple>

namespace voxen
{

// Depth of 11 would require at worst (2^11)^3 = 8'589'934'592 leaves, which overflows uint32_t
static constexpr uint8_t MAX_ALLOWED_DEPTH = 10;

std::pair<uint32_t, ChunkOctreeCell *> ChunkOctree::allocCell(uint8_t depth)
{
	assert(depth <= MAX_ALLOWED_DEPTH);

	uint32_t idx;
	if (!m_free_cells.empty()) {
		idx = m_free_cells.back();
		m_free_cells.pop_back();
	} else {
		idx = uint32_t(m_cells.size());
		m_cells.emplace_back();
	}

	ChunkOctreeCell *cell = &m_cells[idx];
	cell->is_leaf = false;
	cell->depth = depth;
	std::fill_n(cell->children_ids, 8, INVALID_NODE_ID);
	return { idx, cell };
}

std::pair<uint32_t, ChunkOctreeLeaf *> ChunkOctree::allocLeaf(uint8_t depth)
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

	ChunkOctreeLeaf *leaf = &m_leaves[idx];
	leaf->is_leaf = true;
	leaf->depth = depth;
	return { idx | LEAF_ID_BIT, leaf };
}

void ChunkOctree::freeNode(uint32_t id)
{
	if (id == INVALID_NODE_ID) {
		return;
	}

	if (isCellId(id)) {
		assert(id < m_cells.size());
		m_free_cells.emplace_back(id);
	} else {
		id ^= LEAF_ID_BIT;
		assert(id < m_leaves.size());
		m_free_leaves.emplace_back(id);
	}
}

void ChunkOctree::clear() noexcept
{
	m_cells.clear();
	m_leaves.clear();
	m_free_cells.clear();
	m_free_leaves.clear();
	m_root_id = INVALID_NODE_ID;
}

ChunkOctreeNodeBase *ChunkOctree::idToPointer(uint32_t id) noexcept
{
	if (id == INVALID_NODE_ID) {
		return nullptr;
	}
	if (isCellId(id)) {
		return &m_cells[id];
	} else {
		return &m_leaves[id ^ LEAF_ID_BIT];
	}
}

const ChunkOctreeNodeBase *ChunkOctree::idToPointer(uint32_t id) const noexcept
{
	if (id == INVALID_NODE_ID) {
		return nullptr;
	}
	if (isCellId(id)) {
		return &m_cells[id];
	} else {
		return &m_leaves[id ^ LEAF_ID_BIT];
	}
}

}
