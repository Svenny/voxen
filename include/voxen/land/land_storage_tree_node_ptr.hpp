#pragma once

#include <voxen/land/land_fwd.hpp>
#include <voxen/world/world_tick_id.hpp>

#include <glm/vec3.hpp>

namespace voxen::land
{

// This is an exposed part of `StorageTree` implementation, basically
// a reference-counting smart pointer with versioning and copy-on-write.
// Do not use this class directly.
template<typename TNode>
class StorageTreeNodePtr {
public:
	StorageTreeNodePtr() = default;
	// Takes node ownership from `other`, leaving null there
	StorageTreeNodePtr(StorageTreeNodePtr &&other) noexcept;
	// Shares node ownership with `other`
	StorageTreeNodePtr(const StorageTreeNodePtr &other) noexcept;
	// Swaps node ownership with `other`
	StorageTreeNodePtr &operator=(StorageTreeNodePtr &&other) noexcept;
	// Proper release of the previous pointer is impossible without `ctl`,
	// so copy assignment is deleted to avoid dangerous situations
	StorageTreeNodePtr &operator=(const StorageTreeNodePtr &other) = delete;
	// NOTE: destructor does NOT release ownership, call `reset()` manually!
	~StorageTreeNodePtr();

	// Construct (default initialize) a node and its user data block. UB if already constructed.
	void init(const StorageTreeControl &ctl, world::TickId tick, glm::ivec3 min_coord);
	// If `tick > tick()`, copy-construct a node and its user data block. Otherwise do nothing.
	void moo(const StorageTreeControl &ctl, world::TickId tick);
	// Release node reference, destroying it if this was the last one
	void reset(const StorageTreeControl &ctl) noexcept;

	world::TickId tick() const noexcept { return m_tick; }
	// Overwrite stored tick; use with caution.
	void set_tick(world::TickId tick) noexcept { m_tick = tick; }

	TNode *get() noexcept { return m_node; }
	TNode *operator->() noexcept { return m_node; }
	TNode &operator*() noexcept { return *m_node; }

	const TNode *get() const noexcept { return m_node; }
	const TNode *operator->() const noexcept { return m_node; }
	const TNode &operator*() const noexcept { return *m_node; }

	operator bool() const noexcept { return m_node != nullptr; }

private:
	world::TickId m_tick = world::TickId::INVALID;
	TNode *m_node = nullptr;
};

extern template class StorageTreeNodePtr<detail::ChunkNode>;
extern template class StorageTreeNodePtr<detail::DuoctreeX4Node>;
extern template class StorageTreeNodePtr<detail::DuoctreeX16Node>;
extern template class StorageTreeNodePtr<detail::DuoctreeX64Node>;
extern template class StorageTreeNodePtr<detail::DuoctreeX256Node>;
extern template class StorageTreeNodePtr<detail::TriquadtreeBridgeNode>;
extern template class StorageTreeNodePtr<detail::TriquadtreeRootNode>;

} // namespace voxen::land
