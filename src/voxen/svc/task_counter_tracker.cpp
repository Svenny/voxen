#include "task_counter_tracker.hpp"

#include <algorithm>
#include <mutex>

namespace voxen::svc::detail
{

uint64_t TaskCounterTracker::allocateCounter() noexcept
{
	return m_next_allocated_counter.fetch_add(1, std::memory_order_relaxed);
}

void TaskCounterTracker::completeCounter(uint64_t counter)
{
	CompletionList &list = m_completion_lists[counter % NUM_COMPLETION_LISTS];

	const uint64_t desired = counter / NUM_COMPLETION_LISTS;
	uint64_t expected = desired - 1;

	std::atomic_uint64_t &fully_completed = list.fully_completed_value;
	if (fully_completed.compare_exchange_strong(expected, desired, std::memory_order_relaxed)) [[likely]] {
		// In-order completion, we're good to go
		return;
	}

	std::lock_guard lock(list.lock);
	auto &segments = list.out_of_order_segments;

	bool appended = false;

	// Try appending to an existing segment.
	// We keep segments sorted as [first; last] tuples in reverse order:
	//    { [A0, B0], [A1, B1], ..., [Ak, Bk] }
	//    Ai <= Bi
	//    A0 > A1 > ... > Ak
	//    B0 > B1 > ... > Bk
	// so after appending we can try collapsing the adjacent segment.
	for (size_t i = 0; i < segments.size(); i++) {
		if (desired + 1 == segments[i].first) {
			segments[i].first = desired;

			// Extended the left bound, try collapsing with the next ("earlier") segment
			if (i + 1 < segments.size() && segments[i + 1].second + 1 == desired) {
				// This segment includes the next segment
				segments[i].first = segments[i + 1].first;
				segments.erase(segments.begin() + ptrdiff_t(i + 1));
			}

			appended = true;
			break;
		}
		if (desired == segments[i].second + 1) {
			segments[i].second = desired;

			// Extended the right bound, try collapsing with the previous ("later") segment
			if (i > 0 && segments[i - 1].first == desired + 1) {
				// The previous segment includes this segment
				segments[i - 1].first = segments[i].first;
				segments.erase(segments.begin() + ptrdiff_t(i));
			}

			appended = true;
			break;
		}
	}

	// Not found a suitable segment, start a new one
	if (!appended) [[unlikely]] {
		// Find the appropriate position to keep it sorted
		auto iter = segments.begin();
		while (iter != segments.end() && iter->second > desired) {
			++iter;
		}

		// This will insert before `iter` which points to either
		// the end or the first segment having `second < desired`
		segments.emplace(iter, desired, desired);
	}

	// Now try to complete segments - remember they are sorted,
	// we can stop as soon as we fail the first attempt.
	while (!segments.empty()) {
		auto [first, last] = segments.back();
		if (first != expected + 1) {
			break;
		}

		if (fully_completed.compare_exchange_strong(expected, last, std::memory_order_relaxed)) [[likely]] {
			expected = last;
			segments.pop_back();
		}
	}
}

bool TaskCounterTracker::isCounterComplete(uint64_t counter) noexcept
{
	CompletionList &list = m_completion_lists[counter % NUM_COMPLETION_LISTS];
	const uint64_t expected = counter / NUM_COMPLETION_LISTS;

	if (list.fully_completed_value.load(std::memory_order_relaxed) >= expected) [[likely]] {
		return true;
	}

	std::lock_guard lock(list.lock);
	auto &segments = list.out_of_order_segments;

	// XXX: segments are sorted so we could use binary search.
	// Not sure if it's profitable (can have few segments) though, needs stats.
	for (size_t i = 0; i < segments.size(); i++) {
		// Check if expected value is inside the segment
		if (segments[i].first <= expected && segments[i].second >= expected) {
			return true;
		}
	}

	return false;
}

size_t TaskCounterTracker::trimCompleteCounters(std::span<uint64_t> counters) noexcept
{
	// XXX: this is likely not the most optimal in terms of shared memory operations.
	// We could first sort counters by `counter / NUM_COMPLETION_LISTS` to aggregate
	// them by respective lists, then do batched checks for whole groups at once.
	//
	// However, this optimization becomes relevant only for large sets of counters,
	// which is not (?) common in practice? Need to collect stats from real workloads.

	size_t remaining = counters.size();

	for (size_t i = 0; i < remaining; /*nothing*/) {
		const uint64_t counter = counters[i];

		CompletionList &list = m_completion_lists[counter % NUM_COMPLETION_LISTS];
		const uint64_t expected = counter / NUM_COMPLETION_LISTS;

		if (list.fully_completed_value.load(std::memory_order_relaxed) >= expected) [[likely]] {
			std::swap(counters[i], counters[remaining - 1]);
			remaining--;
			continue;
		}

		bool has_in_out_of_order = false;

		{
			std::lock_guard lock(list.lock);
			auto &segments = list.out_of_order_segments;

			for (size_t j = 0; j < segments.size(); j++) {
				// Check if expected value is inside the segment
				if (segments[j].first <= expected && segments[j].second >= expected) {
					has_in_out_of_order = true;
					break;
				}
			}
		}

		if (has_in_out_of_order) {
			std::swap(counters[i], counters[remaining - 1]);
			remaining--;
		} else {
			i++;
		}
	}

	return remaining;
}

} // namespace voxen::svc::detail
