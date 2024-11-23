#include <voxen/land/land_storage_tree.hpp>

#include "land_storage_tree_private.hpp"
#include "storage_tree_utils_private.hpp"

#include <cassert>

namespace voxen::land
{

StorageTree::StorageTree(StorageTreeControl ctl) noexcept : m_ctl(ctl) {}

StorageTree::StorageTree(StorageTree &&other) noexcept = default;
StorageTree::StorageTree(const StorageTree &other) noexcept = default;

StorageTree &StorageTree::operator=(StorageTree &&other) noexcept
{
	for (size_t i = 0; i < std::size(m_root_items); i++) {
		// This will effectively swap pointers
		m_root_items[i] = std::move(other.m_root_items[i]);
	}

	// Swap control blocks so `other` will destroy its (former our) pointers with the correct dtor
	std::swap(m_ctl, other.m_ctl);
	return *this;
}

StorageTree &StorageTree::operator=(const StorageTree &other) noexcept
{
	for (size_t i = 0; i < std::size(m_root_items); i++) {
		m_root_items[i].reset(m_ctl);
		// Pointer has no copy assignment, manually swap with a temporary object
		m_root_items[i] = StorageTreeNodePtr<detail::TriquadtreeRootNode>(other.m_root_items[i]);
	}

	m_ctl = other.m_ctl;
	return *this;
}

StorageTree::~StorageTree()
{
	for (auto &item : m_root_items) {
		item.reset(m_ctl);
	}
}

void StorageTree::copyFrom(const StorageTree &other, UserDataCopyFn user_data_copy_fn, void *user_fn_ctx)
{
	for (size_t i = 0; i < std::size(m_root_items); i++) {
		if (m_root_items[i].tick() < other.m_root_items[i].tick()) {
			// TODO: implement me
			(void) user_data_copy_fn;
			(void) user_fn_ctx;
		}
	}
}

void *StorageTree::access(uint64_t tree_path, WorldTickId tick)
{
	const uint32_t root_index = static_cast<uint32_t>(tree_path >> (64 - 8));
	auto &root_item = m_root_items[root_index];

	if (!root_item) [[unlikely]] {
		root_item.init(m_ctl, tick, StorageTreeUtils::calcRootItemMinCoord(root_index));
	} else {
		root_item.moo(m_ctl, tick);
	}

	return root_item->access(m_ctl, tree_path, tick);
}

void StorageTree::remove(uint64_t tree_path, WorldTickId tick)
{
	uint32_t root_index = static_cast<uint32_t>(tree_path >> (64 - 8));
	auto &root_item = m_root_items[root_index];
	if (root_item) [[likely]] {
		root_item.moo(m_ctl, tick);
		root_item->remove(m_ctl, tree_path, tick);

		if (root_item->empty()) {
			root_item.reset(m_ctl);
		}
	}
}

void *StorageTree::lookup(uint64_t tree_path) noexcept
{
	// Casting const away looks not very nice, but the lookup logic is exactly the same
	const StorageTree *me = this;
	return const_cast<void *>(me->lookup(tree_path));
}

const void *StorageTree::lookup(uint64_t tree_path) const noexcept
{
	auto &root_item = m_root_items[tree_path >> (64 - 8)];
	if (root_item) [[likely]] {
		return root_item->lookup(tree_path);
	}

	return nullptr;
}

} // namespace voxen::land
