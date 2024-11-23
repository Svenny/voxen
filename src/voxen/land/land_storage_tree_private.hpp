#pragma once

#include <voxen/land/chunk_key.hpp>
#include <voxen/land/land_storage_tree_node_ptr.hpp>

#include <glm/vec3.hpp>

#include <atomic>
#include <cstdint>

namespace voxen::land::detail
{

struct NodeBase {
	NodeBase() = default;
	NodeBase(NodeBase &&) = delete;
	NodeBase(const NodeBase &other) noexcept : m_live_key_mask(other.m_live_key_mask) {}
	NodeBase &operator=(NodeBase &&) = delete;
	NodeBase &operator=(const NodeBase &) = delete;
	~NodeBase() = default;

	void addRef() noexcept { m_ref_count.fetch_add(1, std::memory_order_relaxed); }
	bool releaseRef() noexcept { return m_ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1; }

protected:
	std::atomic_uint32_t m_ref_count = 1;
	// Used only by duoctree nodes, always zero in chunk and triquadtree nodes.
	// Duoctree nodes track keys inserted into the node.
	// Bits 0:7 store "subnode" bits for odd-scale keys, bit 8 denotes the even-scale key.
	uint32_t m_live_key_mask = 0;
};

template<typename TChild>
struct DuoctreeNodeBase : NodeBase {
	constexpr static int32_t NODE_SIZE_CHUNKS = 4 * TChild::NODE_SIZE_CHUNKS;
	constexpr static uint32_t NODE_SCALE_LOG2 = TChild::NODE_SCALE_LOG2 + 2;
	constexpr static uint32_t TREE_PATH_BYTE = TChild::TREE_PATH_BYTE + 1;
	constexpr static bool IS_DUOCTREE_NODE = true;

	using ChildItem = StorageTreeNodePtr<TChild>;

	explicit DuoctreeNodeBase(glm::ivec3 min_coord) noexcept : m_key(min_coord, NODE_SCALE_LOG2) {}
	DuoctreeNodeBase(DuoctreeNodeBase &&other) = delete;
	DuoctreeNodeBase(const DuoctreeNodeBase &other) noexcept;
	DuoctreeNodeBase &operator=(DuoctreeNodeBase &&) = delete;
	DuoctreeNodeBase &operator=(const DuoctreeNodeBase &) = delete;
	~DuoctreeNodeBase();

	ChildItem *item(size_t storage_index) noexcept
	{
		return std::launder(reinterpret_cast<ChildItem *>(m_storage + storage_index * sizeof(ChildItem)));
	}

	const ChildItem *item(size_t storage_index) const noexcept
	{
		return std::launder(reinterpret_cast<const ChildItem *>(m_storage + storage_index * sizeof(ChildItem)));
	}

	ChunkKey key() const noexcept { return m_key; }
	bool empty() const noexcept { return m_live_key_mask == 0 && m_child_mask == 0; }

	void *userStorage() noexcept { return this + 1; }
	const void *userStorage() const noexcept { return this + 1; }

	void clear(const StorageTreeControl &ctl) noexcept;
	void *access(const StorageTreeControl &ctl, uint64_t tree_path, WorldTickId tick);
	void remove(const StorageTreeControl &ctl, uint64_t tree_path, WorldTickId tick);
	const void *lookup(uint64_t tree_path) const noexcept;

protected:
	const ChunkKey m_key;
	uint64_t m_child_mask = 0;
	alignas(ChildItem) std::byte m_storage[64 * sizeof(ChildItem)];

	ChildItem *constructItem(size_t storage_index, size_t after_count) noexcept;
	void removeItem(size_t storage_index, size_t after_count) noexcept;
};

template<bool HILO, typename TChild>
struct TriquadtreeNodeBase : NodeBase {
	constexpr static int32_t NODE_SIZE_CHUNKS = 8 * TChild::NODE_SIZE_CHUNKS;
	constexpr static uint32_t TREE_PATH_BYTE = TChild::TREE_PATH_BYTE + 1;
	constexpr static bool IS_DUOCTREE_NODE = false;

	using ChildItem = StorageTreeNodePtr<TChild>;

	explicit TriquadtreeNodeBase(glm::ivec3 min_coord) noexcept : m_min_x(min_coord.x), m_min_z(min_coord.z) {}
	TriquadtreeNodeBase(TriquadtreeNodeBase &&other) = delete;
	TriquadtreeNodeBase(const TriquadtreeNodeBase &other) noexcept;
	TriquadtreeNodeBase &operator=(TriquadtreeNodeBase &&) = delete;
	TriquadtreeNodeBase &operator=(const TriquadtreeNodeBase &) = delete;
	~TriquadtreeNodeBase();

	ChildItem *item(size_t storage_index) noexcept
	{
		return std::launder(reinterpret_cast<ChildItem *>(m_storage + storage_index * sizeof(ChildItem)));
	}

	const ChildItem *item(size_t storage_index) const noexcept
	{
		return std::launder(reinterpret_cast<const ChildItem *>(m_storage + storage_index * sizeof(ChildItem)));
	}

	bool empty() const noexcept { return m_child_mask[0] == 0 && m_child_mask[NUM_MASKS - 1] == 0; }

	void clear(const StorageTreeControl &ctl) noexcept;
	void *access(const StorageTreeControl &ctl, uint64_t tree_path, WorldTickId tick);
	void remove(const StorageTreeControl &ctl, uint64_t tree_path, WorldTickId tick);
	const void *lookup(uint64_t tree_path) const noexcept;

protected:
	constexpr static size_t NUM_MASKS = HILO ? 2 : 1;

	const int32_t m_min_x;
	const int32_t m_min_z;
	uint64_t m_child_mask[NUM_MASKS] = {};
	alignas(ChildItem) std::byte m_storage[64 * NUM_MASKS * sizeof(ChildItem)];

	ChildItem *constructItem(size_t storage_index, size_t after_count) noexcept;
	void removeItem(size_t storage_index, size_t after_count) noexcept;
};

struct ChunkNode : NodeBase {
	constexpr static int32_t NODE_SIZE_CHUNKS = 1;
	constexpr static uint32_t NODE_SCALE_LOG2 = 0;
	constexpr static uint32_t TREE_PATH_BYTE = 0;
	constexpr static bool IS_DUOCTREE_NODE = false;

	explicit ChunkNode(glm::ivec3 min_coord) noexcept : m_key(min_coord, NODE_SCALE_LOG2) {}
	ChunkNode(ChunkNode &&other) = delete;
	ChunkNode(const ChunkNode &other) = default;
	ChunkNode &operator=(ChunkNode &&) = delete;
	ChunkNode &operator=(const ChunkNode &) = delete;

	ChunkKey key() const noexcept { return m_key; }

	void *userStorage() noexcept { return this + 1; }
	const void *userStorage() const noexcept { return this + 1; }

	void clear(const StorageTreeControl &) noexcept {}

protected:
	const ChunkKey m_key;
};

struct DuoctreeX4Node : DuoctreeNodeBase<ChunkNode> {
	using DuoctreeNodeBase::DuoctreeNodeBase;
};

struct DuoctreeX16Node : DuoctreeNodeBase<DuoctreeX4Node> {
	using DuoctreeNodeBase::DuoctreeNodeBase;
};

struct DuoctreeX64Node : DuoctreeNodeBase<DuoctreeX16Node> {
	using DuoctreeNodeBase::DuoctreeNodeBase;
};

struct DuoctreeX256Node : DuoctreeNodeBase<DuoctreeX64Node> {
	using DuoctreeNodeBase::DuoctreeNodeBase;
};

using DuoctreeLargestNode = DuoctreeX256Node;

struct TriquadtreeBridgeNode : TriquadtreeNodeBase<true, DuoctreeLargestNode> {
	using TriquadtreeNodeBase::TriquadtreeNodeBase;
};

struct TriquadtreeRootNode : TriquadtreeNodeBase<false, TriquadtreeBridgeNode> {
	using TriquadtreeNodeBase::TriquadtreeNodeBase;
};

extern template struct DuoctreeNodeBase<ChunkNode>;
extern template struct DuoctreeNodeBase<DuoctreeX4Node>;
extern template struct DuoctreeNodeBase<DuoctreeX16Node>;
extern template struct DuoctreeNodeBase<DuoctreeX64Node>;

extern template struct TriquadtreeNodeBase<true, DuoctreeLargestNode>;
extern template struct TriquadtreeNodeBase<false, TriquadtreeBridgeNode>;

} // namespace voxen::land::detail
