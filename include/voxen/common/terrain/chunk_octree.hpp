#pragma once

#include <voxen/common/terrain/config.hpp>
#include <voxen/common/terrain/qef_solver.hpp>
#include <voxen/util/allocator.hpp>

#include <array>
#include <cassert>
#include <vector>

namespace voxen
{

struct ChunkOctreeLeaf;
struct ChunkOctreeCell;

struct ChunkOctreeNodeBase {
	bool is_leaf;
	int8_t depth;

	ChunkOctreeCell *castToCell() noexcept { assert(!is_leaf); return reinterpret_cast<ChunkOctreeCell *>(this); }
	const ChunkOctreeCell *castToCell() const noexcept { assert(!is_leaf); return reinterpret_cast<const ChunkOctreeCell *>(this); }
	ChunkOctreeLeaf *castToLeaf() noexcept { assert(is_leaf); return reinterpret_cast<ChunkOctreeLeaf *>(this); }
	const ChunkOctreeLeaf *castToLeaf() const noexcept { assert(is_leaf); return reinterpret_cast<const ChunkOctreeLeaf *>(this); }
};

struct ChunkOctreeCell : public ChunkOctreeNodeBase {
	uint32_t children_ids[8];
};
static_assert(sizeof(ChunkOctreeCell) == 36, "36-byte ChunkOctreeCell packing is broken");

struct ChunkOctreeLeaf : public ChunkOctreeNodeBase {
	glm::vec3 surface_vertex;
	glm::vec3 surface_normal;
	uint32_t surface_vertex_id;
	std::array<voxel_t, 8> corners;
	QefSolver3D::State qef_state;
};
static_assert(sizeof(ChunkOctreeLeaf) == 96, "96-byte ChunkOctreeLeaf packing is broken");

// TODO (Svenny): remove diagnostics disabling
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
static_assert(offsetof(ChunkOctreeCell, is_leaf) == offsetof(ChunkOctreeLeaf, is_leaf),
              "Dynamic ChunkOctree[Cell|Leaf] typing is broken");
#pragma GCC diagnostic pop

class ChunkOctree {
public:
	static constexpr uint32_t INVALID_NODE_ID = UINT32_MAX;
	static constexpr uint32_t LEAF_ID_BIT = uint32_t(1) << uint32_t(31);

	ChunkOctree() = default;
	ChunkOctree(ChunkOctree &&) = default;
	ChunkOctree(const ChunkOctree &) = default;
	ChunkOctree &operator = (ChunkOctree &&) = default;
	ChunkOctree &operator = (const ChunkOctree &) = default;
	~ChunkOctree() = default;

	[[nodiscard]] std::pair<uint32_t, ChunkOctreeCell *> allocCell(int8_t depth);
	[[nodiscard]] std::pair<uint32_t, ChunkOctreeLeaf *> allocLeaf(int8_t depth);
	void freeNode(uint32_t idx);
	void clear() noexcept;

	// WARNING: all node pointers are invalidated after calling `allocCell` or `allocLeaf`
	ChunkOctreeNodeBase *idToPointer(uint32_t id) noexcept;
	const ChunkOctreeNodeBase *idToPointer(uint32_t id) const noexcept;

	uint32_t baseRoot() const noexcept { return m_base_root_id; }
	void setBaseRoot(uint32_t id) noexcept { m_base_root_id = id; }
	uint32_t extendedRoot() const noexcept { return m_ext_root_id; }
	void setExtendedRoot(uint32_t id) noexcept { m_ext_root_id = id; }

	static bool isCellId(uint32_t id) noexcept { return (id & LEAF_ID_BIT) == 0; }
	static bool isLeafId(uint32_t id) noexcept { return (id & LEAF_ID_BIT) != 0; }

private:
	template<typename T>
	using Vector = std::vector<T, DomainAllocator<T, AllocationDomain::TerrainOctree>>;

	Vector<ChunkOctreeCell> m_cells;
	Vector<ChunkOctreeLeaf> m_leaves;

	Vector<uint32_t> m_free_cells;
	Vector<uint32_t> m_free_leaves;

	uint32_t m_base_root_id = INVALID_NODE_ID;
	uint32_t m_ext_root_id = INVALID_NODE_ID;
};

}
