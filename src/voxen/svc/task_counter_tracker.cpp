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

	uint64_t desired = counter / NUM_COMPLETION_LISTS;
	uint64_t expected = desired - 1;

	auto &fully_completed = list.fully_completed_value;
	if (fully_completed.compare_exchange_strong(expected, desired, std::memory_order_relaxed)) [[likely]] {
		// In-order completion, we're good to go
		return;
	}

	std::lock_guard lock(list.lock);
	auto &values = list.out_of_order_values;
	values.emplace_back(desired);

	if (values.size() > 1) {
		std::sort(values.rbegin(), values.rend());
		while (!values.empty() && values.back() == expected + 1) {
			if (fully_completed.compare_exchange_strong(expected, values.back(), std::memory_order_relaxed)) [[likely]] {
				expected = values.back();
				values.pop_back();
			}
		}
	}
}

bool TaskCounterTracker::isCounterComplete(uint64_t counter) noexcept
{
	CompletionList &list = m_completion_lists[counter % NUM_COMPLETION_LISTS];
	uint64_t expected = counter / NUM_COMPLETION_LISTS;

	if (list.fully_completed_value.load(std::memory_order_relaxed) >= expected) [[likely]] {
		return true;
	}

	std::lock_guard lock(list.lock);
	auto iter = std::find(list.out_of_order_values.begin(), list.out_of_order_values.end(), expected);
	return iter != list.out_of_order_values.end();
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
			auto iter = std::find(list.out_of_order_values.begin(), list.out_of_order_values.end(), expected);
			has_in_out_of_order = (iter != list.out_of_order_values.end());
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
