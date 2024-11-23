#include "land_storage_tree_private.hpp"

#include "storage_tree_utils_private.hpp"

#include <bit>

namespace voxen::land::detail
{

namespace
{

size_t popcount(uint64_t mask) noexcept
{
	return static_cast<size_t>(std::popcount(mask));
}

size_t popcount(uint64_t (&mask)[1]) noexcept
{
	return popcount(mask[0]);
}

size_t popcount(uint64_t (&mask)[2]) noexcept
{
	return popcount(mask[0]) + popcount(mask[1]);
}

} // namespace

// --- DuoctreeNodeBase ---

template<typename TChild>
DuoctreeNodeBase<TChild>::DuoctreeNodeBase(const DuoctreeNodeBase &other) noexcept
	: m_key(other.m_key), m_child_mask(other.m_child_mask)
{
	size_t count = popcount(m_child_mask);

	for (size_t i = 0; i < count; i++) {
		new (item(i)) ChildItem(*other.item(i));
	}
}

template<typename TChild>
DuoctreeNodeBase<TChild>::~DuoctreeNodeBase()
{
	// Needs `StorageTreeControl` to properly destroy nodes, `reset()` must be used
	assert(m_child_mask == 0);
}

template<typename TChild>
void DuoctreeNodeBase<TChild>::clear(const StorageTreeControl &ctl) noexcept
{
	size_t count = popcount(m_child_mask);

	for (size_t i = 0; i < count; i++) {
		item(i)->reset(ctl);
		item(i)->~ChildItem();
	}

	m_child_mask = 0;
}

template<typename TChild>
void *DuoctreeNodeBase<TChild>::access(const StorageTreeControl &ctl, uint64_t tree_path, WorldTickId tick)
{
	const uint64_t my_component = StorageTreeUtils::extractNodePathComponent<TREE_PATH_BYTE>(tree_path);
	const uint64_t child_bit = StorageTreeUtils::extractNodePathChildBit(my_component);

	if (StorageTreeUtils::extractNodePathStopBit(my_component)) {
		// Stop bit set at our level
		m_live_key_mask |= StorageTreeUtils::extractNodeKeyMaskBit(tree_path, my_component);
		return userStorage();
	}

	size_t storage_index = popcount(m_child_mask & (child_bit - 1));

	if (m_child_mask & child_bit) [[likely]] {
		ChildItem &child = *item(storage_index);
		child.moo(ctl, tick);

		if constexpr (std::is_same_v<TChild, ChunkNode>) {
			return child->userStorage();
		} else {
			return child->access(ctl, tree_path, tick);
		}
	}

	size_t after_count = popcount(m_child_mask & ~(child_bit - 1));

	ChildItem &child = *constructItem(storage_index, after_count);

	glm::ivec3 child_min_coord;
	const uint64_t child_id = my_component & 63u;
	child_min_coord.x = m_key.x + TChild::NODE_SIZE_CHUNKS * int32_t((child_id % 16) / 4);
	child_min_coord.y = m_key.y + TChild::NODE_SIZE_CHUNKS * int32_t(child_id / 16);
	child_min_coord.z = m_key.z + TChild::NODE_SIZE_CHUNKS * int32_t(child_id % 4);

	try {
		child.init(ctl, tick, child_min_coord);
		m_child_mask |= child_bit;
	}
	catch (...) {
		removeItem(storage_index, after_count);
		throw;
	}

	if constexpr (std::is_same_v<TChild, ChunkNode>) {
		return child->userStorage();
	} else {
		return child->access(ctl, tree_path, tick);
	}
}

template<typename TChild>
void DuoctreeNodeBase<TChild>::remove(const StorageTreeControl &ctl, uint64_t tree_path, WorldTickId tick)
{
	const uint64_t my_component = StorageTreeUtils::extractNodePathComponent<TREE_PATH_BYTE>(tree_path);
	const uint64_t child_bit = StorageTreeUtils::extractNodePathChildBit(my_component);

	if (StorageTreeUtils::extractNodePathStopBit(my_component)) {
		// Stop bit set at our level
		m_live_key_mask &= ~StorageTreeUtils::extractNodeKeyMaskBit(tree_path, my_component);
		return;
	}

	if (!(m_child_mask & child_bit)) [[unlikely]] {
		return;
	}

	size_t storage_index = popcount(m_child_mask & (child_bit - 1));
	size_t after_count = popcount(m_child_mask & ~(child_bit - 1));

	ChildItem &child = *item(storage_index);
	child.moo(ctl, tick);

	if constexpr (std::is_same_v<TChild, ChunkNode>) {
		child.reset(ctl);
		removeItem(storage_index, after_count);
		m_child_mask ^= child_bit;
	} else {
		child->remove(ctl, tree_path, tick);

		if (child->empty()) {
			child.reset(ctl);
			removeItem(storage_index, after_count);
			m_child_mask ^= child_bit;
		}
	}
}

template<typename TChild>
const void *DuoctreeNodeBase<TChild>::lookup(uint64_t tree_path) const noexcept
{
	const uint64_t my_component = StorageTreeUtils::extractNodePathComponent<TREE_PATH_BYTE>(tree_path);
	const uint64_t child_bit = StorageTreeUtils::extractNodePathChildBit(my_component);

	if (StorageTreeUtils::extractNodePathStopBit(my_component)) {
		// Stop bit set at our level
		const uint32_t target_key_bit = StorageTreeUtils::extractNodeKeyMaskBit(tree_path, my_component);
		return (m_live_key_mask & target_key_bit) ? userStorage() : nullptr;
	}

	if (!(m_child_mask & child_bit)) [[unlikely]] {
		return nullptr;
	}

	uint64_t before_mask = m_child_mask & (child_bit - 1);
	const ChildItem &child = *item(popcount(before_mask));

	if constexpr (std::is_same_v<TChild, ChunkNode>) {
		return child->userStorage();
	} else {
		return child->lookup(tree_path);
	}
}

template<typename TChild>
auto DuoctreeNodeBase<TChild>::constructItem(size_t storage_index, size_t after_count) noexcept -> ChildItem *
{
	ChildItem *move_to = item(storage_index + after_count);
	ChildItem *move_from = move_to - 1;

	// Default-construct an empty item at the end
	new (move_to) ChildItem();

	// Move items left-to-right "opening the gap"
	for (size_t i = 0; i < after_count; i++) {
		*move_to = std::move(*move_from);
		move_to = move_from--;
	}

	return move_to;
}

template<typename TChild>
void DuoctreeNodeBase<TChild>::removeItem(size_t storage_index, size_t after_count) noexcept
{
	ChildItem *move_to = item(storage_index);
	ChildItem *move_from = move_to + 1;

	// Move items right-to-left "closing the gap"
	for (size_t i = 0; i < after_count; i++) {
		*move_to = std::move(*move_from);
		move_to = move_from++;
	}

	move_to->~ChildItem();
}

// Implement template instantiations
template struct DuoctreeNodeBase<ChunkNode>;
template struct DuoctreeNodeBase<DuoctreeX4Node>;
template struct DuoctreeNodeBase<DuoctreeX16Node>;
template struct DuoctreeNodeBase<DuoctreeX64Node>;

// --- TriquadtreeNodeBase ---

template<bool HILO, typename TChild>
TriquadtreeNodeBase<HILO, TChild>::TriquadtreeNodeBase(const TriquadtreeNodeBase &other) noexcept
	: m_min_x(other.m_min_x), m_min_z(other.m_min_z)
{
	std::copy(std::begin(other.m_child_mask), std::end(other.m_child_mask), m_child_mask);
	size_t count = popcount(m_child_mask);

	for (size_t i = 0; i < count; i++) {
		new (item(i)) ChildItem(*other.item(i));
	}
}

template<bool HILO, typename TChild>
TriquadtreeNodeBase<HILO, TChild>::~TriquadtreeNodeBase()
{
	// Needs `StorageTreeControl` to properly destroy nodes, `reset()` must be used
	assert(popcount(m_child_mask) == 0);
}

template<bool HILO, typename TChild>
void TriquadtreeNodeBase<HILO, TChild>::clear(const StorageTreeControl &ctl) noexcept
{
	size_t count = popcount(m_child_mask);

	for (size_t i = 0; i < count; i++) {
		item(i)->reset(ctl);
		item(i)->~ChildItem();
	}

	std::fill(std::begin(m_child_mask), std::end(m_child_mask), 0);
}

template<bool HILO, typename TChild>
void *TriquadtreeNodeBase<HILO, TChild>::access(const StorageTreeControl &ctl, uint64_t tree_path, WorldTickId tick)
{
	const uint64_t my_component = StorageTreeUtils::extractNodePathComponent<TREE_PATH_BYTE>(tree_path);
	const uint64_t child_bit = StorageTreeUtils::extractNodePathChildBit(my_component);
	const bool y_negative = StorageTreeUtils::triquadtreeYNegative(my_component);

	uint64_t mask = m_child_mask[0];
	size_t storage_offset = 0;
	size_t after_count = 0;

	if constexpr (HILO) {
		if (y_negative) {
			mask = m_child_mask[1];
			storage_offset = popcount(m_child_mask[0]);
		} else {
			after_count = popcount(m_child_mask[1]);
		}
	}

	size_t storage_index = storage_offset + popcount(mask & (child_bit - 1));

	if (mask & child_bit) [[likely]] {
		ChildItem &child = *item(storage_index);
		child.moo(ctl, tick);
		return child->access(ctl, tree_path, tick);
	}

	after_count += popcount(mask & ~(child_bit - 1));

	ChildItem &child = *constructItem(storage_index, after_count);

	glm::ivec3 child_min_coord;
	const uint64_t child_id = my_component & 63u;
	child_min_coord.x = m_min_x + TChild::NODE_SIZE_CHUNKS * int32_t(child_id / 8);
	child_min_coord.y = y_negative ? -TChild::NODE_SIZE_CHUNKS : 0;
	child_min_coord.z = m_min_z + TChild::NODE_SIZE_CHUNKS * int32_t(child_id % 8);

	try {
		child.init(ctl, tick, child_min_coord);

		if constexpr (HILO) {
			if (y_negative) {
				m_child_mask[1] |= child_bit;
			} else {
				m_child_mask[0] |= child_bit;
			}
		} else {
			m_child_mask[0] |= child_bit;
		}
	}
	catch (...) {
		removeItem(storage_index, after_count);
		throw;
	}

	return child->access(ctl, tree_path, tick);
}

template<bool HILO, typename TChild>
void TriquadtreeNodeBase<HILO, TChild>::remove(const StorageTreeControl &ctl, uint64_t tree_path, WorldTickId tick)
{
	const uint64_t my_component = StorageTreeUtils::extractNodePathComponent<TREE_PATH_BYTE>(tree_path);
	const uint64_t child_bit = StorageTreeUtils::extractNodePathChildBit(my_component);
	const bool y_negative = StorageTreeUtils::triquadtreeYNegative(my_component);

	uint64_t *mask = &m_child_mask[0];
	size_t storage_offset = 0;
	size_t after_count = 0;

	if constexpr (HILO) {
		if (y_negative) {
			mask = &m_child_mask[1];
			storage_offset = popcount(m_child_mask[0]);
		} else {
			after_count = popcount(m_child_mask[1]);
		}
	}

	if (!(*mask & child_bit)) [[unlikely]] {
		return;
	}

	size_t storage_index = storage_offset + popcount(*mask & (child_bit - 1));

	ChildItem &child = *item(storage_index);
	child.moo(ctl, tick);
	child->remove(ctl, tree_path, tick);

	if (child->empty()) {
		child.reset(ctl);

		after_count += popcount(*mask & ~(child_bit - 1));
		removeItem(storage_index, after_count);

		*mask ^= child_bit;
	}
}

template<bool HILO, typename TChild>
const void *TriquadtreeNodeBase<HILO, TChild>::lookup(uint64_t tree_path) const noexcept
{
	const uint64_t my_component = StorageTreeUtils::extractNodePathComponent<TREE_PATH_BYTE>(tree_path);
	const uint64_t child_bit = StorageTreeUtils::extractNodePathChildBit(my_component);
	const bool y_negative = StorageTreeUtils::triquadtreeYNegative(my_component);

	uint64_t mask = m_child_mask[0];
	size_t storage_offset = 0;

	if constexpr (HILO) {
		if (y_negative) {
			mask = m_child_mask[1];
			storage_offset = popcount(m_child_mask[0]);
		}
	}

	if (!(mask & child_bit)) [[unlikely]] {
		return nullptr;
	}

	uint64_t before_mask = mask & (child_bit - 1);
	return (*item(storage_offset + popcount(before_mask)))->lookup(tree_path);
}

template<bool HILO, typename TChild>
auto TriquadtreeNodeBase<HILO, TChild>::constructItem(size_t storage_index, size_t after_count) noexcept -> ChildItem *
{
	ChildItem *move_to = item(storage_index + after_count);
	ChildItem *move_from = move_to - 1;

	// Default-construct an empty item at the end
	new (move_to) ChildItem();

	// Move items left-to-right "opening the gap"
	for (size_t i = 0; i < after_count; i++) {
		*move_to = std::move(*move_from);
		move_to = move_from--;
	}

	return move_to;
}

template<bool HILO, typename TChild>
void TriquadtreeNodeBase<HILO, TChild>::removeItem(size_t storage_index, size_t after_count) noexcept
{
	ChildItem *move_to = item(storage_index);
	ChildItem *move_from = move_to + 1;

	// Move items right-to-left "closing the gap"
	for (size_t i = 0; i < after_count; i++) {
		*move_to = std::move(*move_from);
		move_to = move_from++;
	}

	move_to->~ChildItem();
}

// Implement template instantiations
template struct TriquadtreeNodeBase<true, DuoctreeLargestNode>;
template struct TriquadtreeNodeBase<false, TriquadtreeBridgeNode>;

} // namespace voxen::land::detail
