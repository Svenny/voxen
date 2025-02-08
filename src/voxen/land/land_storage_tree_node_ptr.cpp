#include <voxen/land/land_storage_tree_node_ptr.hpp>

#include <voxen/land/storage_tree_common.hpp>

#include "land_storage_tree_private.hpp"

#include <cassert>
#include <utility>

namespace voxen::land
{

namespace
{

template<typename TNode>
size_t selectNodeSize(const StorageTreeControl& ctl) noexcept
{
	if constexpr (std::is_same_v<TNode, detail::ChunkNode>) {
		return sizeof(TNode) + ctl.chunk_user_data_size;
	} else if constexpr (TNode::IS_DUOCTREE_NODE) {
		return sizeof(TNode) + ctl.duoctree_user_data_size;
	} else {
		return sizeof(TNode);
	}
}

template<typename TNode>
constexpr bool NODE_HAS_USER_STORAGE = std::is_same_v<TNode, detail::ChunkNode> || TNode::IS_DUOCTREE_NODE;

template<typename TNode>
	requires NODE_HAS_USER_STORAGE<TNode>
void userDataDefaultCtor(const StorageTreeControl& ctl, TNode* node)
{
	if constexpr (TNode::IS_DUOCTREE_NODE) {
		ctl.duoctree_user_data_default_ctor(ctl.user_fn_ctx, node->key(), node->userStorage());
	} else {
		ctl.chunk_user_data_default_ctor(ctl.user_fn_ctx, node->key(), node->userStorage());
	}
}

template<typename TNode>
	requires NODE_HAS_USER_STORAGE<TNode>
void userDataCopyCtor(const StorageTreeControl& ctl, TNode* node, TNode* copy_from)
{
	if constexpr (TNode::IS_DUOCTREE_NODE) {
		ctl.duoctree_user_data_copy_ctor(ctl.user_fn_ctx, node->key(), node->userStorage(), copy_from->userStorage());
	} else {
		ctl.chunk_user_data_copy_ctor(ctl.user_fn_ctx, node->key(), node->userStorage(), copy_from->userStorage());
	}
}

template<typename TNode>
	requires NODE_HAS_USER_STORAGE<TNode>
void userDataDtor(const StorageTreeControl& ctl, TNode* node) noexcept
{
	if constexpr (TNode::IS_DUOCTREE_NODE) {
		ctl.duoctree_user_data_dtor(ctl.user_fn_ctx, node->key(), node->userStorage());
	} else {
		ctl.chunk_user_data_dtor(ctl.user_fn_ctx, node->key(), node->userStorage());
	}
}

} // namespace

template<typename TNode>
StorageTreeNodePtr<TNode>::StorageTreeNodePtr(StorageTreeNodePtr&& other) noexcept
	: m_tick(std::exchange(other.m_tick, world::TickId::INVALID)), m_node(std::exchange(other.m_node, nullptr))
{}

template<typename TNode>
StorageTreeNodePtr<TNode>::StorageTreeNodePtr(const StorageTreeNodePtr& other) noexcept
	: m_tick(other.m_tick), m_node(other.m_node)
{
	if (m_node) {
		m_node->addRef();
	}
}

template<typename TNode>
StorageTreeNodePtr<TNode>& StorageTreeNodePtr<TNode>::operator=(StorageTreeNodePtr<TNode>&& other) noexcept
{
	std::swap(m_tick, other.m_tick);
	std::swap(m_node, other.m_node);
	return *this;
}

template<typename TNode>
StorageTreeNodePtr<TNode>::~StorageTreeNodePtr()
{
	// Pointer needs `StorageTreeControl` to properly destroy nodes
	assert(!m_node);
}

template<typename TNode>
void StorageTreeNodePtr<TNode>::init(const StorageTreeControl& ctl, world::TickId tick, glm::ivec3 min_coord)
{
	assert(!m_node);

	void* storage = ::operator new(selectNodeSize<TNode>(ctl));
	TNode* new_node = new (storage) TNode(min_coord);

	if constexpr (NODE_HAS_USER_STORAGE<TNode>) {
		try {
			userDataDefaultCtor<TNode>(ctl, new_node);
		}
		catch (...) {
			new_node->clear(ctl);
			new_node->~TNode();
			::operator delete(storage);
			throw;
		}
	}

	m_tick = tick;
	m_node = new_node;
}

template<typename TNode>
void StorageTreeNodePtr<TNode>::moo(const StorageTreeControl& ctl, world::TickId tick)
{
	if (m_tick >= tick) {
		return;
	}

	if (!m_node) [[unlikely]] {
		m_tick = tick;
		return;
	}

	void* storage = ::operator new(selectNodeSize<TNode>(ctl));
	TNode* new_node = new (storage) TNode(*m_node);

	if constexpr (NODE_HAS_USER_STORAGE<TNode>) {
		try {
			userDataCopyCtor<TNode>(ctl, new_node, m_node);
		}
		catch (...) {
			new_node->clear(ctl);
			new_node->~TNode();
			::operator delete(storage);
			throw;
		}
	}

	reset(ctl);

	m_tick = tick;
	m_node = new_node;
}

template<typename TNode>
void StorageTreeNodePtr<TNode>::reset(const StorageTreeControl& ctl) noexcept
{
	if (!m_node) {
		return;
	}

	if (m_node->releaseRef()) {
		if constexpr (NODE_HAS_USER_STORAGE<TNode>) {
			userDataDtor<TNode>(ctl, m_node);
		}

		m_node->clear(ctl);
		m_node->~TNode();
		::operator delete(m_node);
	}

	m_node = nullptr;
}

template class StorageTreeNodePtr<detail::ChunkNode>;
template class StorageTreeNodePtr<detail::DuoctreeX4Node>;
template class StorageTreeNodePtr<detail::DuoctreeX16Node>;
template class StorageTreeNodePtr<detail::DuoctreeX64Node>;
template class StorageTreeNodePtr<detail::DuoctreeX256Node>;
template class StorageTreeNodePtr<detail::TriquadtreeBridgeNode>;
template class StorageTreeNodePtr<detail::TriquadtreeRootNode>;

} // namespace voxen::land
