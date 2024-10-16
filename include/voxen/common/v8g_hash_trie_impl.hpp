#pragma once

#include <voxen/common/v8g_hash_trie.hpp>

#include <algorithm>
#include <bit>
#include <cassert>

namespace voxen
{

template<CV8gUniqueHashableKey Key, CV8gValue Value>
class V8gHashTrie<Key, Value>::Node {
public:
	// Space for at least two `Item`s is needed to resolve hash prefix
	// collisions without node expansion immediately following allocation.
	constexpr static uint32_t INITIAL_CAPACITY = 2 * std::max(sizeof(NodeItem), sizeof(Item));
	// We don't need to ever allocate for more than 64 of the larger item
	constexpr static uint32_t MAX_CAPACITY = 64 * std::max(sizeof(NodeItem), sizeof(Item));

	// Initial constructor, makes an empty node.
	// Dynamic struct size (see `m_bytes`) must be `sizeof(Node) + capacity`.
	Node(uint32_t capacity, uint32_t consumed_hash_bits) noexcept
		: m_capacity_bytes(capacity), m_consumed_hash_bits(consumed_hash_bits)
	{
		// Sanity check structure layout
		static_assert(alignof(Node) == alignof(uint64_t));
		// `Item` is assumed to be the larger of two elements
		static_assert(sizeof(NodeItem) <= sizeof(Item));

		// Ensure we don't have to care about padding
		static_assert(offsetof(Node, m_bytes) % alignof(uint64_t) == 0);
		static_assert(alignof(Item) <= alignof(uint64_t));
		static_assert(alignof(NodeItem) <= alignof(uint64_t));

		// Ensure 32 bits of storage counter is enough to fit everything
		// (128 just to have headroom for arithmetic without overflow)
		static_assert(sizeof(Item) * 128 <= UINT32_MAX);
	}

	// Copy constructor.
	// Dynamic struct size (see `m_bytes`) must be `sizeof(Node) + capacity`.
	// Additionally, `capacity` must be not less than `old.m_used_bytes`.
	Node(const Node &old, uint32_t capacity) noexcept
		: m_node_bitmap(old.m_node_bitmap)
		, m_item_bitmap(old.m_item_bitmap)
		, m_used_bytes(old.m_used_bytes)
		, m_capacity_bytes(capacity)
		, m_consumed_hash_bits(old.m_consumed_hash_bits)
	{
		int32_t node_count = std::popcount(m_node_bitmap);
		NodeItem *new_node_ptr = &refNodeIndex(0);
		const NodeItem *old_node_ptr = &old.refNodeIndex(0);

		for (int32_t i = 0; i < node_count; i++) {
			new (new_node_ptr + i) NodeItem(old_node_ptr[i]);
		}

		int32_t item_count = std::popcount(m_item_bitmap);
		Item *new_item_ptr = &refItemIndex(0);
		const Item *old_item_ptr = &old.refItemIndex(0);

		for (int32_t i = 0; i < item_count; i++) {
			new (new_item_ptr - i) Item(old_item_ptr[-i]);
		}
	}

	Node(Node &&) = delete;
	Node(const Node &) = delete;
	Node &operator=(Node &&) = delete;
	Node &operator=(const Node &) = delete;

	~Node() noexcept
	{
		int32_t node_count = std::popcount(m_node_bitmap);
		for (int32_t i = 0; i < node_count; i++) {
			refNodeIndex(i).~NodeItem();
		}

		int32_t item_count = std::popcount(m_item_bitmap);
		for (int32_t i = 0; i < item_count; i++) {
			refItemIndex(i).~Item();
		}
	}

	// Insert `NodeItem` indexed by `bit`, returns reference to the inserted item.
	// `bit` must be set in neither `m_node_bitmap` nor `m_item_bitmap`.
	// Free space precondition: `m_used_bytes + sizeof(NodeItem) <= m_capacity_bytes`.
	NodeItem &insertNode(uint64_t bit, NodeItem item) noexcept
	{
		int32_t target_index = std::popcount(m_node_bitmap & (bit - 1));
		int32_t after_nodes = std::popcount(m_node_bitmap & ~(bit - 1));

		NodeItem *node_ptr = &refNodeIndex(target_index);

		if (after_nodes > 0) {
			new (node_ptr + after_nodes) NodeItem(std::move(node_ptr[after_nodes - 1]));
			after_nodes--;

			while (after_nodes > 0) {
				node_ptr[after_nodes] = std::move(node_ptr[after_nodes - 1]);
				after_nodes--;
			}

			*node_ptr = std::move(item);
		} else {
			new (node_ptr) NodeItem(std::move(item));
		}

		m_node_bitmap |= bit;
		m_used_bytes += sizeof(NodeItem);

		return *node_ptr;
	}

	// Erase `NodeItem` indexed by `bit`, which must be set in `m_node_bitmap`
	void eraseNode(uint64_t bit) noexcept
	{
		int32_t target_index = std::popcount(m_node_bitmap & (bit - 1));
		// Count includes the node to be removed
		int32_t after_nodes = std::popcount(m_node_bitmap & ~(bit - 1));

		NodeItem *move_to_ptr = &refNodeIndex(target_index);

		// Shift all "after" nodes left to cover the "hole"
		for (int32_t i = 0; i < after_nodes - 1; i++) {
			*move_to_ptr = std::move(*(move_to_ptr + 1));
			move_to_ptr++;
		}

		// Destroy the now last (rightmost) node
		move_to_ptr->~NodeItem();

		m_node_bitmap ^= bit;
		m_used_bytes -= sizeof(NodeItem);
	}

	// Insert `Item` indexed by `bit`.
	// `bit` must be set in neither `m_node_bitmap` nor `m_item_bitmap`.
	// Free space precondition: `m_used_bytes + sizeof(Item) <= m_capacity_bytes`.
	void insertItem(uint64_t bit, Item item) noexcept
	{
		int32_t target_index = std::popcount(m_item_bitmap & (bit - 1));
		int32_t after_items = std::popcount(m_item_bitmap & ~(bit - 1));

		Item *item_ptr = &refItemIndex(target_index);

		if (after_items > 0) {
			Item *move_to_ptr = item_ptr - after_items;

			new (move_to_ptr) Item(std::move(*(move_to_ptr + 1)));
			move_to_ptr++;

			while (move_to_ptr != item_ptr) {
				*move_to_ptr = std::move(*(move_to_ptr + 1));
				move_to_ptr++;
			}

			*item_ptr = std::move(item);
		} else {
			new (item_ptr) Item(std::move(item));
		}

		m_item_bitmap |= bit;
		m_used_bytes += sizeof(Item);
	}

	// Erase `Item` indexed by `bit`, which must be set in `m_item_bitmap`
	void eraseItem(uint64_t bit) noexcept
	{
		int32_t target_index = std::popcount(m_item_bitmap & (bit - 1));
		// Count includes item to be removed
		int32_t after_items = std::popcount(m_item_bitmap & ~(bit - 1));

		Item *move_to_ptr = &refItemIndex(target_index);

		// Shift all "after" items right to cover the "hole"
		for (int32_t i = 0; i < after_items - 1; i++) {
			*move_to_ptr = std::move(*(move_to_ptr - 1));
			move_to_ptr--;
		}

		// Destroy the now last (leftmost) item
		move_to_ptr->~Item();

		m_item_bitmap ^= bit;
		m_used_bytes -= sizeof(Item);
	}

	// Convert `Item` indexed by `bit` into a `NodeItem` containing this `Item`.
	// `bit` must be set in `m_item_bitmap`.
	// No free space preconditions because `NodeItem` is not larger than `Item`.
	NodeItem *promoteItemToNode(uint64_t bit)
	{
		Item &ref_item = refItemBit(bit);

		// Create node item first - the only potentially failing action; then shuffle stuff around
		NodeItem node_item(ref_item.version(), allocateNode(m_consumed_hash_bits + 6));
		// Move item away - we're erasing it from this node
		Item item = std::move(ref_item);

		// Erase item and insert node in its place
		eraseItem(bit);
		NodeItem &ref_node_item = insertNode(bit, std::move(node_item));

		// Item goes one level deeper, need to know more hash bits
		uint64_t hash_bits = (item.key().hash() << (m_consumed_hash_bits + 6)) >> (64 - 6);
		uint64_t child_bit = uint64_t(1) << hash_bits;

		// `node_item` is moved away, use ref to inserted one now
		ref_node_item.second->insertItem(child_bit, std::move(item));

		return &ref_node_item;
	}

	// Try shrinking `NodeItem` indexed by `bit`, which must be set in `m_node_bitmap`.
	// If it contains just one item, then `NodeItem` can be demoted to just `Item`.
	// If it is empty we can remove it altogether.
	void tryShrinkChildNode(uint64_t bit) noexcept
	{
		Node &child_node = *refNodeBit(bit).second;

		if (child_node.m_node_bitmap != 0) {
			// Can't shrink more than one level
			return;
		}

		if (child_node.m_item_bitmap == 0) {
			// No bits in both masks - node is empty and can be dropped
			eraseNode(bit);
			return;
		}

		if (m_capacity_bytes - m_used_bytes + sizeof(NodeItem) < sizeof(Item)) {
			// We won't have enough free space to replace `NodeItem` with `Item`
			return;
		}

		if ((child_node.m_item_bitmap & (child_node.m_item_bitmap - 1)) == 0) {
			// Only one bit set in item mask - one item, demote to parent node.
			// We know its index can be nothing but 0.
			Item item = std::move(child_node.refItemIndex(0));
			// Drop this child...
			eraseNode(bit);
			// ...and insert that item in its place.
			insertItem(bit, std::move(item));
		}
	}

	// Return pointer to the first item (with smallest hash) stored in subtree
	const Item *findFirstItem() const noexcept
	{
		// Bits do not intersect, can combine and iterate over both
		uint64_t combo_mask = m_node_bitmap | m_item_bitmap;
		int32_t node_index = 0;

		// Iterate over set mask bits from LSB to MSB
		while (combo_mask != 0) {
			uint64_t bit = uint64_t(1) << std::countr_zero(combo_mask);

			if (m_item_bitmap & bit) {
				// Item - it is automatically the first one.
				// Note we can't just return it before the loop as there can be child nodes earlier.
				return &refItemIndex(0);
			}

			// Child node - recurse to it; it is expected to have an item
			if (const Item *found = refNodeIndex(node_index).second->findFirstItem(); found != nullptr) [[likely]] {
				return found;
			}

			combo_mask ^= bit;
			node_index++;
		}

		return nullptr;
	}

	// Return pointer to the "next" item (in hash order) stored in subtree after one with `hash_bits`
	const Item *findNextItem(uint64_t hash_bits) const noexcept
	{
		// Check items indexed by `bit` separately - their logic is a bit different
		uint64_t bit = uint64_t(1) << (hash_bits >> (64 - 6));

		if (m_node_bitmap & bit) {
			// Node indexed by `bit` - recurse to it consuming some hash bits
			const Item *found = refNodeBit(bit).second->findNextItem(hash_bits << 6);

			if (found != nullptr) {
				return found;
			}
		}

		if (m_item_bitmap & bit) {
			// Item indexed by `bit` - compare hashes by order
			const Item &item = refItemBit(bit);

			// We've already lost first `m_consumed_hash_bits` of `hash_bits`
			// but we know they must be equal to those of `item.key().hash()`
			uint64_t item_hash_bits = item.key().hash() << m_consumed_hash_bits;
			if (hash_bits < item_hash_bits) {
				// Yes, this is the "next" item
				return &item;
			}

			// No, find any other item after this
		}

		// Now it's enough to find the first item.

		// Bits do not intersect, can combine and iterate over both.
		// Remove all entries at and below `bit` - they can't be "next".
		// If `bit == 2^63` the mask will be cleared to zero, which is expected.
		uint64_t combo_mask = (m_node_bitmap | m_item_bitmap) & ~((bit << 1) - 1);
		// Count in all nodes skipped over
		int32_t node_index = std::popcount(m_node_bitmap & ~combo_mask);

		// Iterate over set mask bits from LSB to MSB.
		// Now it's just enough to return the first item.
		while (combo_mask != 0) {
			bit = uint64_t(1) << std::countr_zero(combo_mask);

			if (m_item_bitmap & bit) {
				// Item - it is automatically the next one.
				// Note we can't just return it before the loop as there can be child nodes earlier.
				return &refItemBit(bit);
			}

			// Child node - recurse to it; it is expected to have an item
			if (const Item *found = refNodeIndex(node_index).second->findFirstItem(); found != nullptr) [[likely]] {
				return found;
			}

			combo_mask ^= bit;
			node_index++;
		}

		return nullptr;
	}

	NodeItem &refNodeIndex(int32_t index) noexcept
	{
		return *std::launder(reinterpret_cast<NodeItem *>(m_bytes) + index);
	}

	const NodeItem &refNodeIndex(int32_t index) const noexcept
	{
		return *std::launder(reinterpret_cast<const NodeItem *>(m_bytes) + index);
	}

	NodeItem &refNodeBit(uint64_t bit) noexcept { return refNodeIndex(std::popcount(m_node_bitmap & (bit - 1))); }

	const NodeItem &refNodeBit(uint64_t bit) const noexcept
	{
		return refNodeIndex(std::popcount(m_node_bitmap & (bit - 1)));
	}

	Item &refItemIndex(int32_t index) noexcept
	{
		return *std::launder(reinterpret_cast<Item *>(m_bytes + m_capacity_bytes) - 1 - index);
	}

	const Item &refItemIndex(int32_t index) const noexcept
	{
		return *std::launder(reinterpret_cast<const Item *>(m_bytes + m_capacity_bytes) - 1 - index);
	}

	Item &refItemBit(uint64_t bit) noexcept { return refItemIndex(std::popcount(m_item_bitmap & (bit - 1))); }

	const Item &refItemBit(uint64_t bit) const noexcept
	{
		return refItemIndex(std::popcount(m_item_bitmap & (bit - 1)));
	}

	static NodePtr allocateNode(uint32_t consumed_hash_bits)
	{
		void *ptr = ::operator new(sizeof(Node) + INITIAL_CAPACITY);
		return NodePtr(new (ptr) Node(INITIAL_CAPACITY, consumed_hash_bits));
	}

	static NodePtr copyNode(const Node &old)
	{
		void *ptr = ::operator new(sizeof(Node) + old.m_capacity_bytes);
		return NodePtr(new (ptr) Node(old, old.m_capacity_bytes));
	}

	static NodePtr expandNode(const Node &old)
	{
		// Capacity x1.5 (clamped to max)
		uint32_t capacity = old.m_capacity_bytes + old.m_capacity_bytes / 2;
		capacity = std::min(capacity, MAX_CAPACITY);

		if (capacity % alignof(Item) != 0) {
			// Round capacity so that `m_bytes + capacity` aligns well
			capacity += alignof(Item) - capacity % alignof(Item);
		}

		void *ptr = ::operator new(sizeof(Node) + capacity);
		return NodePtr(new (ptr) Node(old, capacity));
	}

	static bool erase(uint64_t timeline, NodeItem &node_item, Key key, uint64_t hash_bits)
	{
		const uint64_t bit = uint64_t(1) << (hash_bits >> (64 - 6));
		hash_bits <<= 6;

		// We're about to alter this node or its children, need to make a copy.
		// Here we rely on the requirement to increase `timeline` between container copies.
		const bool need_copy = node_item.first != timeline;

		Node *node = node_item.second.get();

		if (node->m_item_bitmap & bit) {
			const Item &item = node->refItemBit(bit);
			if (item.key() != key) {
				return false;
			}

			if (need_copy) {
				node_item.second = copyNode(*node);
				node_item.first = timeline;
				node = node_item.second.get();
			}

			node->eraseItem(bit);
			return true;
		}

		if (!(node->m_node_bitmap & bit)) {
			return false;
		}

		if (need_copy) {
			// This might be a bit too early - we don't know yet if anything
			// will be actually erased. But otherwise we'll have to make it two-phase,
			// first check if anything below should be modified, then COW and do it.
			// Altering pointers without COW-ing first will result in a data race.
			node_item.second = copyNode(*node);
			node_item.first = timeline;
			node = node_item.second.get();
		}

		bool erased = erase(timeline, node->refNodeBit(bit), key, hash_bits);

		// Try shrinking the subtree if it just got smaller
		if (erased) {
			node->tryShrinkChildNode(bit);
		}

		return erased;
	}

	bool visitUnary(extras::function_ref<bool(const Item *)> visitor) const
	{
		uint64_t combo_mask = m_node_bitmap | m_item_bitmap;

		const NodeItem *node_ptr = &refNodeIndex(0);
		const Item *item_ptr = &refItemIndex(0);

		while (combo_mask) {
			uint64_t bit = uint64_t(1) << std::countr_zero(combo_mask);

			if (m_node_bitmap & bit) {
				if (!node_ptr->second->visitUnary(visitor)) [[unlikely]] {
					return false;
				}

				node_ptr++;
			} else {
				if (!visitor(item_ptr)) [[unlikely]] {
					return false;
				}

				item_ptr--;
			}

			combo_mask ^= bit;
		}

		return true;
	}

	static bool visitDiffItemOrdered(const Item *a, const Item *b,
		extras::function_ref<bool(const Item *, const Item *)> visitor)
	{
		if (a->key() == b->key()) {
			if (a->version() != b->version()) {
				return visitor(a, b);
			}

			return true;
		}

		// Select call order - unfortunately have to compute hashes here.
		// Rely on && short-circuit to exit if the first visitor returns false.

		if (a->key().hash() < b->key().hash()) {
			return visitor(a, nullptr) && visitor(nullptr, b);
		}

		return visitor(nullptr, b) && visitor(a, nullptr);
	}

	bool visitDiffItem(const Item *item,
		extras::function_ref<bool(const Item *node_item, const Item *passed_item)> visitor) const
	{
		auto unary_adapter = [&](const Item *callee) { return visitor(callee, nullptr); };

		const uint64_t item_bit = uint64_t(1) << ((item->key().hash() << m_consumed_hash_bits) >> (64 - 6));

		uint64_t combo_mask = m_node_bitmap | m_item_bitmap;

		const NodeItem *node_ptr = &refNodeIndex(0);
		const Item *item_ptr = &refItemIndex(0);
		bool visited_item_bit = false;

		while (combo_mask) {
			uint64_t bit = uint64_t(1) << std::countr_zero(combo_mask);

			if (bit == item_bit) {
				visited_item_bit = true;

				if (m_node_bitmap & bit) {
					if (!node_ptr->second->visitDiffItem(item, visitor)) [[unlikely]] {
						return false;
					}
					node_ptr++;
				} else {
					if (!visitDiffItemOrdered(item_ptr, item, visitor)) [[unlikely]] {
						return false;
					}
					item_ptr--;
				}

				combo_mask ^= bit;
				continue;
			} else if (bit > item_bit && !visited_item_bit) {
				visited_item_bit = true;
				if (!visitor(nullptr, item)) [[unlikely]] {
					return false;
				}
			}

			if (m_node_bitmap & bit) {
				if (!node_ptr->second->visitUnary(unary_adapter)) [[unlikely]] {
					return false;
				}
				node_ptr++;
			} else {
				if (!visitor(item_ptr, nullptr)) [[unlikely]] {
					return false;
				}
				item_ptr--;
			}

			combo_mask ^= bit;
		}

		return visited_item_bit ? true : visitor(nullptr, item);
	}

	static bool visitDiff(const Node *new_node, const Node *old_node, DiffVisitorFn visitor)
	{
		uint64_t new_node_bitmap = new_node->m_node_bitmap;
		uint64_t new_item_bitmap = new_node->m_item_bitmap;
		uint64_t old_node_bitmap = old_node->m_node_bitmap;
		uint64_t old_item_bitmap = old_node->m_item_bitmap;

		auto handle_iteration = [&](uint64_t bit) -> bool {
			int case_id = 0;

			if (new_node_bitmap & bit) {
				case_id += 3;
			} else if (!(new_item_bitmap & bit)) {
				case_id += 6;
			}

			if (old_node_bitmap & bit) {
				case_id += 1;
			} else if (!(old_item_bitmap & bit)) {
				case_id += 2;
			}

			switch (case_id) {
			case 0: { // [0+0] new_item && old_item
				auto &new_item = new_node->refItemBit(bit);
				auto &old_item = old_node->refItemBit(bit);
				return visitDiffItemOrdered(&new_item, &old_item, visitor);
			}
			case 1: { // [0+1] new_item && old_node
				auto adapter = [&](const Item *node_item, const Item *passed_item) {
					return visitor(passed_item, node_item);
				};
				return old_node->refNodeBit(bit).second->visitDiffItem(&new_node->refItemBit(bit), adapter);
			}
			case 2: { // [0+2] new_item && nothing
				return visitor(&new_node->refItemBit(bit), nullptr);
			}
			case 3: { // [3+0] new_node && old_item
				// No need for adapter - order is correct
				return new_node->refNodeBit(bit).second->visitDiffItem(&old_node->refItemBit(bit), visitor);
			}
			case 4: { // [3+1] new_node && old_node
				auto &new_child = new_node->refNodeBit(bit);
				auto &old_child = old_node->refNodeBit(bit);

				if (new_child.first != old_child.first) {
					return visitDiff(new_child.second.get(), old_child.second.get(), visitor);
				}

				return true;
			}
			case 5: { // [3+2] new_node && nothing
				auto adapter = [&](const Item *item) { return visitor(item, nullptr); };
				return new_node->refNodeBit(bit).second->visitUnary(adapter);
			}
			case 6: { // [6+0] nothing && old_item
				return visitor(nullptr, &old_node->refItemBit(bit));
			}
			case 7: { // [6+1] nothing && old_node
				auto adapter = [&](const Item *item) { return visitor(nullptr, item); };
				return old_node->refNodeBit(bit).second->visitUnary(adapter);
			}
			default: // [6+2] nothing && nothing (unreachable)
				__builtin_unreachable();
				assert(false);
				return false;
			}
		};

		uint64_t new_combo_mask = new_node_bitmap | new_item_bitmap;
		uint64_t old_combo_mask = old_node_bitmap | old_item_bitmap;
		uint64_t combo_mask = new_combo_mask | old_combo_mask;

		while (combo_mask) {
			uint64_t bit = uint64_t(1) << std::countr_zero(combo_mask);

			if (!handle_iteration(bit)) [[unlikely]] {
				return false;
			}

			combo_mask ^= bit;
		}

		return true;
	}

// Suppress "flexible array members are a C99 feature"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc99-extensions"

	// Bitmap of stored `NodeItem`s, one mask bit per hash 6-bit part
	uint64_t m_node_bitmap = 0;
	// Bitmap of stored `Item`s, one mask bit per hash 6-bit part.
	// Mutually exclusive with `m_node_bitmap`, can't have the same bit set in both.
	uint64_t m_item_bitmap = 0;
	// Number of storage bytes used by constructed items
	uint64_t m_used_bytes : 28 = 0;
	// Number of storage bytes avaliable (effectively struct size minus header)
	uint64_t m_capacity_bytes : 28 = 0;
	// Number of hash bits consumed to reach this node, not including indexing inside the node.
	uint64_t m_consumed_hash_bits : 8 = 0;
	// Inline storage bytes, organized in double-stack fashion.
	// `NodeItem`s are stored in low part (bottom to top).
	// `Items` are in high part and in reversed order (top to bottom).
	std::byte m_bytes[];

#pragma clang diagnostic pop
};

template<CV8gUniqueHashableKey Key, CV8gValue Value>
V8gHashTrie<Key, Value>::V8gHashTrie(V8gHashTrie &&other) noexcept : m_size(other.m_size)
{
	for (uint32_t i = 0; i < std::size(m_root_nodes); i++) {
		m_root_nodes[i] = std::move(other.m_root_nodes[i]);
		other.m_root_nodes[i] = {};
	}

	other.m_size = 0;
}

template<CV8gUniqueHashableKey Key, CV8gValue Value>
auto V8gHashTrie<Key, Value>::operator=(V8gHashTrie &&other) noexcept -> V8gHashTrie &
{
	for (uint32_t i = 0; i < std::size(m_root_nodes); i++) {
		m_root_nodes[i] = std::move(other.m_root_nodes[i]);
		other.m_root_nodes[i] = {};
	}

	m_size = std::exchange(other.m_size, 0);
	return *this;
}

template<CV8gUniqueHashableKey Key, CV8gValue Value>
auto V8gHashTrie<Key, Value>::operator=(const V8gHashTrie &other) noexcept -> V8gHashTrie &
{
	for (uint32_t i = 0; i < std::size(m_root_nodes); i++) {
		// Check before copying to avoid unnecessary refcount operations
		if (m_root_nodes[i] != other.m_root_nodes[i]) {
			m_root_nodes[i] = other.m_root_nodes[i];
		}
	}

	m_size = other.m_size;
	return *this;
}

template<CV8gUniqueHashableKey Key, CV8gValue Value>
void V8gHashTrie<Key, Value>::insert(uint64_t timeline, Key key, ValuePtr value_ptr)
{
	uint64_t hash_bits = key.hash();

	NodeItem *current_node_item = &m_root_nodes[hash_bits >> (64 - ROOT_NODES_LOG2)];
	hash_bits <<= ROOT_NODES_LOG2;

	// Allocate the root child node if it's null yet
	if (!current_node_item->second) {
		current_node_item->second = Node::allocateNode(ROOT_NODES_LOG2);
		current_node_item->first = timeline;
	}

	// We require absense of hash collisions from user.
	// With this precondition the loop is guaranteed to terminate.
	while (true) {
		Node *current_node = current_node_item->second.get();

		if (current_node_item->first != timeline) {
			// We're about to alter this node or its children, need to make a copy.
			// Here we rely on the requirement to increase `timeline` between container copies.
			current_node_item->second = Node::copyNode(*current_node);
			current_node_item->first = timeline;
			current_node = current_node_item->second.get();
		}

		// Get index bit, consume 6 hash bits
		const uint64_t bit = uint64_t(1) << (hash_bits >> (64 - 6));
		hash_bits <<= 6;

		if (current_node->m_node_bitmap & bit) {
			// This index is stored in a child node, proceed to it
			current_node_item = &current_node->refNodeBit(bit);
			continue;
		}

		if (current_node->m_item_bitmap & bit) {
			// This index is stored directly, check if we have hash prefix collision
			Item &item = current_node->refItemBit(bit);

			if (item.key() == key) {
				// No prefix collision, it's the item we look for
				item.version() = timeline;
				item.valuePtr() = std::move(value_ptr);
				return;
			}

			// Hash prefix collision, need to promote item to a node.
			// There is always enough capacity, we ensure `sizeof(NodeItem) <= sizeof(Item)`.
			current_node_item = current_node->promoteItemToNode(bit);
			// Proceed to the newly created node
			continue;
		}

		// This index is not stored, insert new direct item pointer

		if (current_node->m_used_bytes + sizeof(Item) > current_node->m_capacity_bytes) {
			// Not enough free capacity, expand the node
			current_node_item->second = Node::expandNode(*current_node);
			current_node = current_node_item->second.get();
		}

		current_node->insertItem(bit, Item(timeline, key, std::move(value_ptr)));
		m_size++;
		return;
	}
}

template<CV8gUniqueHashableKey Key, CV8gValue Value>
void V8gHashTrie<Key, Value>::erase(uint64_t timeline, Key key)
{
	uint64_t hash_bits = key.hash();
	NodeItem &root_node_item = m_root_nodes[hash_bits >> (64 - ROOT_NODES_LOG2)];
	hash_bits <<= ROOT_NODES_LOG2;

	if (root_node_item.second && Node::erase(timeline, root_node_item, key, hash_bits)) {
		m_size--;
	}
}

template<CV8gUniqueHashableKey Key, CV8gValue Value>
auto V8gHashTrie<Key, Value>::find(Key key) const noexcept -> const Item *
{
	uint64_t hash_bits = key.hash();

	const Node *current_node = m_root_nodes[hash_bits >> (64 - ROOT_NODES_LOG2)].second.get();
	hash_bits <<= ROOT_NODES_LOG2;

	if (!current_node) [[unlikely]] {
		return nullptr;
	}

	while (true) {
		const uint64_t bit = uint64_t(1) << (hash_bits >> (64 - 6));
		hash_bits <<= 6;

		if (current_node->m_item_bitmap & bit) {
			const Item &item = current_node->refItemBit(bit);
			return item.key() == key ? &item : nullptr;
		}

		if (!(current_node->m_node_bitmap & bit)) {
			return nullptr;
		}

		current_node = current_node->refNodeBit(bit).second.get();
	}
}

template<CV8gUniqueHashableKey Key, CV8gValue Value>
auto V8gHashTrie<Key, Value>::findFirst() const noexcept -> const Item *
{
	for (const NodeItem &item : m_root_nodes) {
		if (!item.second) [[unlikely]] {
			continue;
		}

		if (const Item *found = item.second->findFirstItem(); found != nullptr) [[likely]] {
			return found;
		}
	}

	return nullptr;
}

template<CV8gUniqueHashableKey Key, CV8gValue Value>
auto V8gHashTrie<Key, Value>::findNext(Key key) const noexcept -> const Item *
{
	uint64_t hash_bits = key.hash();

	uint64_t root_index = (hash_bits >> (64 - ROOT_NODES_LOG2));
	hash_bits <<= ROOT_NODES_LOG2;

	const Node *current_node = m_root_nodes[root_index].second.get();
	if (current_node) [[likely]] {
		if (const Item *found = current_node->findNextItem(hash_bits); found != nullptr) [[likely]] {
			return found;
		}
	}

	for (root_index++; root_index < std::size(m_root_nodes); root_index++) {
		current_node = m_root_nodes[root_index].second.get();

		if (!current_node) [[unlikely]] {
			continue;
		}

		if (const Item *found = current_node->findFirstItem(); found != nullptr) [[likely]] {
			return found;
		}
	}

	return nullptr;
}

template<CV8gUniqueHashableKey Key, CV8gValue Value>
void V8gHashTrie<Key, Value>::visitDiff(const V8gHashTrie &old, DiffVisitorFn visitor) const
{
	auto left_unary_adapter = [&](const Item *item) { return visitor(item, nullptr); };
	auto right_unary_adapter = [&](const Item *item) { return visitor(nullptr, item); };

	for (uint32_t i = 0; i < std::size(m_root_nodes); i++) {
		if (m_root_nodes[i].first == old.m_root_nodes[i].first) {
			continue;
		}

		const Node *new_node = m_root_nodes[i].second.get();
		const Node *old_node = old.m_root_nodes[i].second.get();

		if (!new_node && !old_node) [[unlikely]] {
			// Shouldn't happen unless both have timeline 0 which is handled above
			continue;
		}

		if (!new_node) {
			if (!old_node->visitUnary(right_unary_adapter)) [[unlikely]] {
				return;
			}
		} else if (!old_node) {
			if (!new_node->visitUnary(left_unary_adapter)) [[unlikely]] {
				return;
			}
		} else if (!Node::visitDiff(new_node, old_node, visitor)) [[unlikely]] {
			return;
		}
	}
}

} // namespace voxen
