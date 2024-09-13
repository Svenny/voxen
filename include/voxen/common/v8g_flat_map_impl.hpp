#pragma once

#include <voxen/common/v8g_flat_map.hpp>

#include <cassert>

namespace voxen
{

// Immutable-only operations

template<CV8gKey Key, CV8gValue Value, V8gStoragePolicy Policy>
template<CV8gCopyableValue<Value> MutValue>
V8gFlatMap<Key, Value, Policy>::V8gFlatMap(const V8gFlatMap<Key, MutValue, V8gStoragePolicy::Copyable> &mut,
	const V8gFlatMap *old)
	requires(IMMUTABLE)
{
	// Leverage our direct access to mutable's underlying storage
	const auto &mut_items = mut.m_items;

	// Try to reuse old items. We have two sorted arrays, and will search for
	// keys from `mut_items` in `old_items` while iterating over `mut_items`.
	// It's enough to keep one ever-advancing pointer into `old_items`.
	const Item *old_item = old ? old->m_items.data() : nullptr;
	const Item *const old_end = old ? (old_item + old->m_items.size()) : nullptr;

	// Create storage using generator lambda
	m_items = Storage(mut_items.size(), [&](void *place, size_t index) {
		// `index` always points to the current mutable item,
		// try to find it in old storage and attempt reusing.
		const auto &[mut_version, mut_key, mut_value_ptr] = mut_items[index];

		// Rewind through all elements with key less than `mut_key`,
		// we won't ever need them because next keys will be larger.
		while (old_item != old_end && old_item->key() < mut_key) {
			old_item++;
		}

		auto make_value_ptr = [](const MutValue &mut, const Value *old) {
			if constexpr (std::is_constructible_v<Value, const MutValue &, const Value *>) {
				return std::make_shared<Value>(mut, old);
			} else {
				static_assert(std::is_constructible_v<Value, const MutValue &>);
				return std::make_shared<Value>(mut);
			}
		};

		ValuePtr new_value_ptr;

		if (old_item != old_end && old_item->key() == mut_key) {
			// Found old item with the same key, try reusing it
			if (old_item->version() == mut_version) {
				// Same version, might reuse the old value
				new_value_ptr = old_item->valuePtr();
			} else {
				// Outdated version, need to copy again.
				// Here, if the value object is a versioned container itself,
				// it will benefit from knowing its previous state too.
				// Two-arg copy construction allows efficient nested versioned containers.
				new_value_ptr = make_value_ptr(*mut_value_ptr, old_item->valueAddr());
			}
		} else {
			// Copy-construct from mutable object, no possibility for reuse
			new_value_ptr = make_value_ptr(*mut_value_ptr, nullptr);
		}

		// Construct the stored item
		new (place) Item(mut_version, mut_key, std::move(new_value_ptr));
	});
}

template<CV8gKey Key, CV8gValue Value, V8gStoragePolicy Policy>
template<CV8gDmgCopyableValue<Value> MutValue>
V8gFlatMap<Key, Value, Policy>::V8gFlatMap(V8gFlatMap<Key, MutValue, V8gStoragePolicy::DmgCopyable> &mut,
	const V8gFlatMap *old)
	requires(IMMUTABLE)
{
	// Leverage our direct access to mutable's underlying storage
	auto &mut_items = mut.m_items;

	// Try to reuse old items. We have two sorted arrays, and will search for
	// keys from `mut_items` in `old_items` while iterating over `mut_items`.
	// It's enough to keep one ever-advancing pointer into `old_items`.
	const Item *old_item = old ? old->m_items.data() : nullptr;
	const Item *const old_end = old ? (old_item + old->m_items.size()) : nullptr;

	// Create storage using generator lambda
	m_items = Storage(mut_items.size(), [&](void *place, size_t index) {
		// `index` always points to the current mutable item,
		// try to find it in old storage and attempt reusing.
		auto &[mut_version, mut_key, mut_value_ptr] = mut_items[index];

		// Rewind through all elements with key less than `mut_key`,
		// we won't ever need them because next keys will be larger.
		while (old_item != old_end && old_item->key() < mut_key) {
			old_item++;
		}

		auto make_value_ptr = [](MutValue &mut, const Value *old) {
			if constexpr (std::is_constructible_v<Value, MutValue &, const Value *>) {
				return std::make_shared<Value>(mut, old);
			} else {
				static_assert(std::is_constructible_v<Value, MutValue &>);
				return std::make_shared<Value>(mut);
			}
		};

		ValuePtr new_value_ptr;

		if (old_item != old_end && old_item->key() == mut_key) {
			// Found old item with the same key, try reusing it
			if (old_item->version() == mut_version) {
				// Same version, might reuse the old value
				new_value_ptr = old_item->valuePtr();
			} else {
				// Outdated version, need to copy again.
				// Here, if the value object is a versioned container itself,
				// it will benefit from knowing its previous state too.
				// Two-arg copy construction allows efficient nested versioned containers.
				new_value_ptr = make_value_ptr(*mut_value_ptr, old_item->valueAddr());
			}
		} else {
			// Copy-construct from mutable object, no possibility for reuse
			new_value_ptr = make_value_ptr(*mut_value_ptr, nullptr);
		}

		// Construct the stored item
		new (place) Item(mut_version, mut_key, std::move(new_value_ptr));
	});
}

template<CV8gKey Key, CV8gValue Value, V8gStoragePolicy Policy>
template<CV8gSharedValue<Value> MutValue>
V8gFlatMap<Key, Value, Policy>::V8gFlatMap(const V8gFlatMap<Key, MutValue, V8gStoragePolicy::Shared> &mut)
	requires(IMMUTABLE)
{
	// Leverage our direct access to mutable's underlying storage
	auto &mut_items = mut.m_items;

	// Create storage using generator lambda
	m_items = Storage(mut_items.size(), [&](void *place, size_t index) {
		// Copy everything from the mutable item, sharing a value storage
		auto &[mut_version, mut_key, mut_value_ptr] = mut_items[index];
		new (place) Item(mut_version, mut_key, mut_value_ptr);
	});
}

// Mutable-only operations

template<CV8gKey Key, CV8gValue Value, V8gStoragePolicy Policy>
void V8gFlatMap<Key, Value, Policy>::insert(uint64_t timeline, Key key, ValuePtr value_ptr)
	requires(MUTABLE)
{
	auto iter = std::ranges::lower_bound(m_items, key, {}, itemKey);
	if (iter != m_items.end() && iter->key() == key) {
		// Replacement of the same key
		assert(iter->version() <= timeline);
		iter->version() = timeline;

		iter->valuePtr() = std::move(value_ptr);
	} else {
		// New item insertion - `iter` points to the first larger key
		m_items.emplace(iter, timeline, key, std::move(value_ptr));
	}
}

template<CV8gKey Key, CV8gValue Value, V8gStoragePolicy Policy>
void V8gFlatMap<Key, Value, Policy>::insert(uint64_t timeline, Key key, Value &&value)
	requires(!SHARED && std::is_nothrow_move_constructible_v<Value>)
{
	auto iter = std::ranges::lower_bound(m_items, key, {}, itemKey);
	if (iter != m_items.end() && iter->key() == key) {
		// Replacement of the same key
		assert(iter->version() <= timeline);
		iter->version() = timeline;

		if constexpr (std::is_nothrow_move_assignable_v<Value>) {
			// Modify value in-place with move assignment
			iter->value() = std::move(value);
		} else {
			// Recreate value pointer
			iter->valuePtr() = makeValuePtr(std::move(value));
		}
	} else {
		// New item insertion - `iter` points to the first larger key
		m_items.emplace(iter, timeline, key, makeValuePtr(std::move(value)));
	}
}

template<CV8gKey Key, CV8gValue Value, V8gStoragePolicy Policy>
void V8gFlatMap<Key, Value, Policy>::erase(Key key) noexcept
	requires(MUTABLE)
{
	auto iter = std::ranges::lower_bound(m_items, key, {}, itemKey);
	if (iter != m_items.end() && iter->key() == key) {
		m_items.erase(iter);
	}
}

template<CV8gKey Key, CV8gValue Value, V8gStoragePolicy Policy>
auto V8gFlatMap<Key, Value, Policy>::erase(ConstIterator iter) noexcept -> ConstIterator
	requires(MUTABLE)
{
	return m_items.erase(iter);
}

template<CV8gKey Key, CV8gValue Value, V8gStoragePolicy Policy>
Value *V8gFlatMap<Key, Value, Policy>::find(uint64_t timeline, Key key) noexcept
	requires(!SHARED)
{
	auto iter = std::ranges::lower_bound(m_items, key, {}, itemKey);
	if (iter != m_items.end() && iter->key() == key) {
		// Update object version
		assert(iter->version() <= timeline);
		iter->version() = timeline;

		return iter->valueAddr();
	}

	return nullptr;
}

// Common operations

template<CV8gKey Key, CV8gValue Value, V8gStoragePolicy Policy>
auto V8gFlatMap<Key, Value, Policy>::find(Key key) const noexcept -> ConstIterator
{
	auto iter = std::ranges::lower_bound(m_items, key, {}, itemKey);
	if (iter != m_items.end() && itemKey(*iter) == key) {
		return iter;
	}

	return m_items.end();
}

template<CV8gKey Key, CV8gValue Value, V8gStoragePolicy Policy>
void V8gFlatMap<Key, Value, Policy>::visitDiff(const V8gFlatMap *old,
	extras::function_ref<bool(Key, const Value *, const Value *)> visitor) const
{
	const Item *old_item = old ? old->m_items.data() : nullptr;
	const Item *const old_end = old ? (old_item + old->m_items.size()) : nullptr;

	for (const auto &[version, key, value_ptr] : m_items) {
		// Rewind through elements with key less than `key`.
		// They were removed in this version, notify the visitor.
		while (old_item != old_end && old_item->key() < key) {
			if (!visitor(old_item->key(), nullptr, old_item->valueAddr())) {
				// Visitor wants to stop
				return;
			}
			old_item++;
		}

		const Value *old_value_ptr = nullptr;

		if (old_item != old_end && old_item->key() == key) {
			// Found an old item with the same key
			if (old_item->version() != version) {
				// Versions differ - notify about the update
				old_value_ptr = old_item->valueAddr();
				old_item++;
			} else {
				// Versions are equal, don't notify
				old_item++;
				continue;
			}
		} // else - this item was added in this version

		if (!visitor(key, value_ptr.get(), old_value_ptr)) {
			// Visitor wants to stop
			return;
		}
	}
}

} // namespace voxen
