#pragma once

#include <voxen/common/v8g_concepts.hpp>
#include <voxen/common/v8g_helpers.hpp>

#include <extras/dyn_array.hpp>
#include <extras/function_ref.hpp>

#include <span>
#include <vector>

namespace voxen
{

// Dynamic array backed, versioning associative (key->value) container.
// This container is intended to be used when the number of objects is small
// (a few tens, up to ~100) but an object's state is too heavy to always copy it.
//
// Elements are stored in sorted order, giving decent O(log n) lookup complexity
// at the expense of quite slow O(n) insertions/deletions. Though it's unlikely to
// matter much when staying in the recommended size range.
//
// See `V8gStoragePolicy` for descriptions of available storage policies.
//
// Supports efficient versioning-optimized operations:
// - Mutable-to-immutable container copy, possibly with value type conversion,
//   possibly reusing unchanged contents from any previous copy.
// - Visit different (added, removed, updated) objects relative to
//   another container made from the same "origin" (see `visitDiff`).
template<CV8gKey Key, CV8gValue Value, V8gStoragePolicy Policy = V8gStoragePolicy::Copyable>
class V8gFlatMap {
public:
	constexpr static bool IMMUTABLE = Policy == V8gStoragePolicy::Immutable;
	constexpr static bool MUTABLE = !IMMUTABLE;
	constexpr static bool SHARED = Policy == V8gStoragePolicy::Immutable || Policy == V8gStoragePolicy::Shared;

	using ValuePtr = std::conditional_t<SHARED, std::shared_ptr<Value>, std::unique_ptr<Value>>;
	using Item = V8gMapItem<Key, ValuePtr>;
	using Storage = std::conditional_t<IMMUTABLE, extras::dyn_array<Item>, std::vector<Item>>;

	using ConstIterator = typename Storage::const_iterator;

	V8gFlatMap() = default;
	V8gFlatMap(V8gFlatMap &&) = default;
	V8gFlatMap &operator=(V8gFlatMap &&) = default;
	~V8gFlatMap() = default;

#pragma region Immutable-only operations

	V8gFlatMap(const V8gFlatMap &)
		requires(IMMUTABLE)
	= default; // Only immutable containers are copy-constructible
	V8gFlatMap &operator=(const V8gFlatMap &)
		requires(IMMUTABLE)
	= default; // Only immutable containers are copy-assignable

	// Optimized copy construction from a mutable container, possibly
	// reusing previous value objects (where their versions did not change).
	//
	// `old`, if not null, must be either empty or constructed as `mut` copy itself;
	// or, at least, version/key/value semantics used to create its "base" mutable container
	// must be "compatible" with those of `mut` (like "same (version, key) => same value").
	// Otherwise the whole versioning logic will not make sense, even though it
	// won't cause undefined behavior, memory leaks or anything like that.
	//
	// When a key exists in `old` but its version is different in `mut`, meaning a value object
	// copy is needed, this constructor will take advantage of its two-argument copy constructor
	// (if present, see `CV8gCopyableValue`). Note, this constructor has exactly that form.
	// It enables efficient and arbitrary nesting of versioning containers as long as their
	// wrapping (value) objects properly forward arguments ("old" immutable container pointers).
	//
	// `old` needs not be the latest copy of `mut`, though using older copies
	// will likely result in additional (and unnecessary) value object copies.
	// Due to the flat (non-hierarchical) nature of this container, copy complexity
	// is still linear in the whole container size, not in the number of updated keys.
	template<CV8gCopyableValue<Value> MutValue>
	explicit V8gFlatMap(const V8gFlatMap<Key, MutValue, V8gStoragePolicy::Copyable> &mut,
		const V8gFlatMap *old = nullptr)
		requires(IMMUTABLE);

	// See regular (copying) constructor description above.
	// The only difference is that mutable container is passed as non-const reference,
	// allowing to alter ("damage") its values.
	//
	// NOTE: this copy can result in incomplete values being stored in this container.
	// This will happen if a previous copy has already "damaged" them.
	// Full state of older (unchanged) value objects can probably only be recovered
	// from the previous copy, so generally `old` should never be null here.
	template<CV8gDmgCopyableValue<Value> MutValue>
	explicit V8gFlatMap(V8gFlatMap<Key, MutValue, V8gStoragePolicy::DmgCopyable> &mut, const V8gFlatMap *old = nullptr)
		requires(IMMUTABLE);

	// Copy construction from a mutable container with shared storage.
	// Does not need the previous container as only pointers are copied,
	// no new value objects are constructed.
	template<CV8gSharedValue<Value> MutValue>
	explicit V8gFlatMap(const V8gFlatMap<Key, MutValue, V8gStoragePolicy::Shared> &mut)
		requires(IMMUTABLE);

#pragma endregion

#pragma region Mutable-only operations

	// Insert an entry into the map.
	// `value_ptr` must be constructed using `makeValuePtr()`. Use that wrapper
	// as pointer type is an implementation detail which might change later.
	//
	// `timeline` value must be greater than or equal to the largest one passed to
	// any prior mutating function call (and strictly greater for `key`), otherwise
	// container behavior is undefined. This applies even if `key` was erased.
	void insert(uint64_t timeline, Key key, ValuePtr value_ptr)
		requires(MUTABLE);

	// Insert an entry, potentially updating it in-place. Might avoid pointer reallocation
	// if the entry is already present; otherwise equal to the first `insert` variant.
	//
	// Not supported with shared storage, it only allows to fully replace the values.
	void insert(uint64_t timeline, Key key, Value &&value)
		requires(!SHARED && std::is_nothrow_move_constructible_v<Value>);

	// Remove an entry from the map.
	//
	// NOTE: even after removing an entry, any future mutating function,
	// including inserting `key` again, must be called with `timeline` value
	// greater than or equal to the largest one passed to any prior call.
	void erase(Key key) noexcept
		requires(MUTABLE);
	ConstIterator erase(ConstIterator iter) noexcept
		requires(MUTABLE);

	// Find value to alter (mutate) it. Returns null if `key` is not found.
	// Not available with shared storage, its values can't be altered after insertion.
	//
	// `timeline` value must be greater than or equal to the largest one passed to
	// any prior mutating function call, otherwise container behavior is undefined.
	Value *find(uint64_t timeline, Key key) noexcept
		requires(!SHARED);

	// Construct value pointer for insertion.
	// Use this wrapper, the underlying pointer type (`std::unique_ptr`/`std::shared_ptr`)
	// is an implementation detail which might change later.
	template<typename... Args>
	static ValuePtr makeValuePtr(Args &&...args)
	{
		if constexpr (SHARED) {
			return std::make_shared<Value>(std::forward<Args>(args)...);
		} else {
			return std::make_unique<Value>(std::forward<Args>(args)...);
		}
	}

#pragma endregion

	ConstIterator find(Key key) const noexcept;

	ConstIterator begin() const noexcept { return m_items.begin(); }
	ConstIterator end() const noexcept { return m_items.end(); }
	std::span<const Item> items() { return m_items; }
	size_t size() const noexcept { return m_items.size(); }

	// Visit every changed (either added, removed or updated version) key-value
	// pair in a pair of containers (two version snapshots). Call visitor
	// functor as per the following pseudocode:
	//
	//     const Value *new_value = /*`this` has `key` ? pointer : nullptr*/
	//     const Value *old_value = /*`old` has `key` ? pointer : nullptr*/
	//     bool proceed = visitor(key, new_value, old_value)
	//     if (!proceed) {
	//         /*stop iteration*/
	//     }
	//
	// Return `false` to stop further iteration - use for early exit.
	// Exception thrown by `visitor` will propagate without any additional effects.
	//
	// Unchanged (same version) key-value pairs are not visited. However, due to the flat
	// (non-hierarchical) nature of this container, visit complexity is still linear in
	// the whole container size, not in the number of updated keys.
	//
	// `old` must be constructed from the same mutable container as `this`,
	// otherwise the whole versioning logic will not make sense.
	// It can also be a null pointer, which is equal to an empty map.
	void visitDiff(const V8gFlatMap *old, extras::function_ref<bool(Key, const Value *, const Value *)> visitor) const;

private:
	Storage m_items;

	static const Key &itemKey(const Item &item) noexcept { return item.key(); }

	// Allow immutable maps to directly access mutables' items
	template<CV8gKey, CV8gValue, V8gStoragePolicy>
	friend class V8gFlatMap;
};

} // namespace voxen
