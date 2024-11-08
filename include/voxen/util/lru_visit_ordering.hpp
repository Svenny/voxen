#pragma once

#include <voxen/util/tagged_tick_id.hpp>

#include <queue>
#include <type_traits>
#include <utility>

namespace voxen
{

// Helper for LRU (least recently updated) key visit ordering based on `std::priority_queue`.
//
// Intended to be used together with a key-value container tracking access timestamps
// to iteratively remove stale keys (those not accessed for a long time) doing a few
// steps for each iteration. Also can help with implementing LRU cache eviction policy.
//
// Stores all key-tick pairs in `std::vector` (as a heap data structure)
// so it has O(n) space overhead. Take this into account if you have MANY keys.
//
// This container provides strong exception safety.
template<typename Key, typename Tag>
class LruVisitOrdering {
public:
	using TickId = TaggedTickId<Tag>;
	using Item = std::pair<TickId, Key>;

	// Add key to be visited on the specified tick.
	// It is valid, though discouraged, to add the same key multiple times.
	//
	// Only add keys when they are first accessed (inserted in the key-value container).
	// Track access timestamps in your main container and update tick IDs here
	// from the visitor callback (see `visitOldest`).
	//
	// Note that the only way to remove a key is by visiting
	// it and returning an invalid tick ID from the callback.
	void addKey(Key key, TickId tick) { m_queue.emplace(tick, std::move(key)); }

	// Apply visitor callback to up to `count` oldest keys or until the queue is empty.
	// Visiting also stops once stored ticks become larger than `tick_cutoff`.
	//
	// Visitor callback should have this or compatible call signature:
	//
	//     TickId callback(const Key &key)
	//
	// If the returned tick ID invalid (i.e. negative value) then the key is removed.
	// Only one key entry is removed if it was added multiple times.
	//
	// Otherwise (valid tick ID returned) the key is re-prioritized to be visited
	// when it becomes the "oldest" again.
	// Note that if the new tick ID is less than or equal to the previously stored one,
	// this key will remain the "oldest" and will get visited again immediately.
	//
	// It is expected that you significantly increase the new tick ID
	// to defer visiting this key again and reduce excessive workload.
	template<typename F>
		requires std::is_invocable_r_v<TickId, F, const Key &>
	void visitOldest(F &&fn, size_t count = 1, TickId tick_cutoff = TickId(INT64_MAX))
	{
		static_assert(std::is_nothrow_move_constructible_v<Key>, "Key must be nothrow move constructible");

		for (size_t i = 0; i < count && !m_queue.empty(); i++) {
			const Item &item = m_queue.top();
			if (item.first > tick_cutoff) {
				// Cutoff reached
				return;
			}

			TickId new_tick = fn(item.second);

			if (new_tick.invalid()) {
				// Tick invalidated, remove this key
				m_queue.pop();
			} else {
				// Re-prioritize this key
				Key moved_key = std::move(m_queue.top().second);
				m_queue.pop();
				m_queue.emplace(new_tick, std::move(moved_key));
			}
		}
	}

	bool empty() const noexcept { return m_queue.empty(); }

private:
	std::priority_queue<Item, std::vector<Item>, std::greater<Item>> m_queue;
};

} // namespace voxen
