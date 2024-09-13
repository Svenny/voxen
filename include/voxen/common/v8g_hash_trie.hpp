#pragma once

#include <voxen/common/v8g_concepts.hpp>
#include <voxen/common/v8g_helpers.hpp>

#include <extras/function_ref.hpp>

#include <memory>

namespace voxen
{

// Hash-indexed trie (not a typo) versioning associative (key->value) container.
//
// IMPORTANT NOTE: container assumes *NO* collisions occur in 64-bit key hashes.
// That means key space can be at most 64-bit, and using bijective hash function
// is strongly recommended, for example `Hash::xxh64Fixed()`.
// This restriction might be lifted with future enhancements.
//
// Trie automatically scales for any size. As long as the key hash distribution
// is good enough (close to uniform), performance of most operations will be similar
// whether there is just a hundred or a million of inserted items.
//
// Essentially this is a 64-ary trie built on hash values, where leaf nodes
// are single key-value pairs. Hashes are consumed in blocks of K (up to 6) bits
// starting from MSB, and select child nodes when traversing the hierarchy.
// This allows for a certain degree of locality optimization. Related keys
// (which are frequently accessed together) might be hashed in such a way
// that their highest 4/6/10/16/... bits become equal. Keep in mind, though,
// that doing so will deteriorate the balance and can easily hurt overall
// performance rather than improve it. Always profile first.
//
// Rehashing happens rarely when expanding the trie, and will always touch
// only a small subset of keys. There are no whole-container rehashings.
// No operation should ever cause lag spikes.
// Memory consumption to performance ratio might be much better than
// that of `std::unordered_map` or other non-hierarchical hash tables,
// this one doesn't need lots of "bucket" allocations to avoid collisions.
//
// Elements are stored in a stable order - sorted by hash values increasing.
// This is mostly an implementation detail but is guaranteed nonetheless.
//
// Key insertion/deletion complexity is O(log n) but these are considerably slower than
// lookup (which is also O(log n)) due to the possible internal storage resizing and trie
// expansion/contraction. This applies to a lesser extent for updates to existing items.
// This structure is designed primarily for lookups, based on the expectation that
// a typical game logic modifies objects far more often than it adds/removes them.
//
// Expect `insert`/`erase` to be 4-5 times slower and `find` to be 2.5-3 times
// slower than `std::unordered_map` based on my very rough measurements.
// Future optimizations might reduce this performance gap.
//
// Supports efficient versioning-optimized operations:
// - Constant-complexity, cheap copy (container snapshot)
// - Visit different (added, removed, updated) objects relative to
//   another snapshot made from the same "origin" (see `visitDiff`).
template<CV8gUniqueHashableKey Key, CV8gValue Value>
class V8gHashTrie {
public:
	using ValuePtr = std::shared_ptr<Value>;
	using Item = V8gMapItem<Key, ValuePtr>;

	using DiffVisitorFn = extras::function_ref<bool(const Item *new_item, const Item *old_item)>;

	V8gHashTrie() = default;
	V8gHashTrie(V8gHashTrie &&other) noexcept;
	// Copy constructor is non-throwing and cheap, essentially
	// only sharing ownership of a few root node pointers
	V8gHashTrie(const V8gHashTrie &other) = default;
	V8gHashTrie &operator=(V8gHashTrie &&other) noexcept;
	// Copy assignment is non-thrwoing and cheap, essentially
	// only sharing ownership of a few root node pointers
	V8gHashTrie &operator=(const V8gHashTrie &other) noexcept;
	~V8gHashTrie() = default;

	// Insert an item into the trie.
	// `value_ptr` must be constructed using `makeValuePtr()`. Use that wrapper
	// as pointer type is an implementation detail which might change later.
	//
	// If `key` is already inserted, `value_ptr` will replace the previously stored
	// value. This is the only intended method of container operation.
	//
	// Pointers to all inserted items are invalidated.
	//
	// NOTE: `key.hash()` must be unique across all inserted keys. This container
	// assumes *total absense* of hash collisions for simplicity/performance.
	//
	// NOTE: `timeline` value must be strictly greater than any value
	// passed to any method since this container was last copied from.
	// Otherwise you will summon race condition demons.
	void insert(uint64_t timeline, Key key, ValuePtr value_ptr);

	// Remove an item from the trie.
	// This operation can modify the container so version is needed.
	//
	// Pointers to all inserted items are invalidated.
	//
	// NOTE: `timeline` value must be strictly greater than any value
	// passed to any method since this container was last copied from.
	// Otherwise you will summon race condition demons.
	void erase(uint64_t timeline, Key key);

	// Find an item in the trie, or null pointer if it's not inserted
	const Item *find(Key key) const noexcept;
	// Returns pointer to the first item in the trie, or null pointer if it's empty.
	// Called `findFirst`, not `begin` for a reason - this operation is non-trivial.
	const Item *findFirst() const noexcept;
	// Returns pointer to the first item in the trie that goes after `key`
	// in hash-sorted order, or null pointer if there is none. `key` itself needs not be inserted.
	// You can use `findFirst()+findNext()` sequence as a replacement.
	const Item *findNext(Key key) const noexcept;

	size_t size() const noexcept { return m_size; }

	// Visit every changed (added, removed or updated version) item in a pair of containers
	// (two version snapshots). Calls visitor functor as per the following pseudocode:
	//
	//     const Item *new_item = /*`this` has `key` ? pointer : nullptr*/
	//     const Item *old_item = /*`old` has `key` ? pointer : nullptr*/
	//     bool proceed = visitor(key, new_item, old_item)
	//     if (!proceed) {
	//         /*stop iteration*/
	//     }
	//
	// Return `false` from `visitor` to stop further iteration (early exit).
	// Thrown exception will propagate without any additional effects.
	//
	// Unchanged (same version) keys are not visited. Due to the hierarchical nature
	// of this container, visit complexity is linear in the number of updated keys
	// (with certain overhead depending on their locality), not the whole container size.
	void visitDiff(const V8gHashTrie &old, DiffVisitorFn visitor) const;

	// Construct value pointer for insertion.
	// Use this wrapper as the underlying pointer type (`std::shared_ptr`)
	// is an implementation detail which might change later.
	template<typename... Args>
	static ValuePtr makeValuePtr(Args &&...args)
	{
		return std::make_shared<Value>(std::forward<Args>(args)...);
	}

private:
	// Some first node pointers are stored inline to slightly reduce indirections
	constexpr static uint32_t ROOT_NODES_LOG2 = 4;

	class Node;

	using NodePtr = std::shared_ptr<Node>;
	using NodeItem = std::pair<uint64_t, NodePtr>;

	size_t m_size = 0;
	NodeItem m_root_nodes[1 << ROOT_NODES_LOG2];
};

} // namespace voxen
