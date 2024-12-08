#pragma once

#include <voxen/common/world_tick_id.hpp>
#include <voxen/land/land_fwd.hpp>
#include <voxen/land/land_public_consts.hpp>
#include <voxen/land/land_storage_tree_node_ptr.hpp>
#include <voxen/land/storage_tree_common.hpp>
#include <voxen/visibility.hpp>

namespace voxen::land
{

class VOXEN_API PrivateStorageTree {
public:
	using UserDataCopyFn = void (*)(void *ctx, ChunkKey key, WorldTickId old_version, WorldTickId new_version,
		void *copy_to, const void *copy_from);

	explicit PrivateStorageTree(StorageTreeControl ctl) noexcept;
	PrivateStorageTree(PrivateStorageTree &&other) noexcept;
	PrivateStorageTree(const PrivateStorageTree &) = delete;
	PrivateStorageTree &operator=(PrivateStorageTree &&other) noexcept;
	PrivateStorageTree &operator=(const PrivateStorageTree &) = delete;
	~PrivateStorageTree();

	void copyFrom(const StorageTree &other, UserDataCopyFn user_data_copy_fn, void *user_fn_ctx);

	void *access(uint64_t tree_path);
	void remove(uint64_t tree_path);

	void *lookup(uint64_t tree_path) noexcept;
	const void *lookup(uint64_t tree_path) const noexcept;

private:
	StorageTreeNodePtr<detail::TriquadtreeRootNode>
		m_root_items[Consts::STORAGE_TREE_ROOT_ITEMS_X * Consts::STORAGE_TREE_ROOT_ITEMS_Z];
	StorageTreeControl m_ctl;
};

} // namespace voxen::land
