#pragma once

#include <voxen/land/land_fwd.hpp>
#include <voxen/land/land_public_consts.hpp>
#include <voxen/land/land_storage_tree_node_ptr.hpp>
#include <voxen/land/storage_tree_common.hpp>
#include <voxen/visibility.hpp>
#include <voxen/world/world_tick_id.hpp>

namespace voxen::land
{

// Type-erased, versioned storage of arbitrary data associated with `ChunkKey`s.
// Do not instantiate this class directly, use `TypedStorageTree`.
class VOXEN_API StorageTree {
public:
	using UserDataCopyFn = void (*)(void *ctx, ChunkKey key, world::TickId old_version, world::TickId new_version,
		void *copy_to, const void *copy_from);

	explicit StorageTree(StorageTreeControl ctl) noexcept;
	StorageTree(StorageTree &&other) noexcept;
	StorageTree(const StorageTree &other) noexcept;
	StorageTree &operator=(StorageTree &&other) noexcept;
	StorageTree &operator=(const StorageTree &other) noexcept;
	~StorageTree();

	// TODO: not yet implemented
	void copyFrom(const StorageTree &other, UserDataCopyFn user_data_copy_fn, void *user_fn_ctx);

	// Find or create node by `tree_path`, returns pointer to its user data block.
	// One duoctree node node can be reached by 9 different paths (1xLn and 8xLn-1).
	//
	// Note that additional duoctree nodes with data blocks can be created while traversing
	// the tree. Their insertion/removal is still tracked correctly - e.g. they will not be
	// returned by `lookup()` until at least one direct `access()` happens to those nodes.
	void *access(uint64_t tree_path, world::TickId tick);
	// Remove node by `tree_path`. For duoctree nodes, insertion/removal of all
	// 9 different possible paths is tracked, and the node (with user data block)
	// is removed only when no inserted paths pointing to this node remain.
	void remove(uint64_t tree_path, world::TickId tick);

	// See `lookup() const`
	void *lookup(uint64_t tree_path) noexcept;
	// Find node by `tree_path`, returns pointer to its user data block or null.
	// Note that even if a duoctree node from this path exists, null will be returned
	// only if exactly this path (not one of 8 others leading to the same node)
	// was inserted by `access()` before and not `remove()`d after that.
	const void *lookup(uint64_t tree_path) const noexcept;

private:
	StorageTreeNodePtr<detail::TriquadtreeRootNode>
		m_root_items[Consts::STORAGE_TREE_ROOT_ITEMS_X * Consts::STORAGE_TREE_ROOT_ITEMS_Z];
	StorageTreeControl m_ctl;
};

} // namespace voxen::land
