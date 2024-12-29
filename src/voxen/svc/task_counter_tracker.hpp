#pragma once

#include <voxen/os/futex.hpp>

#include <extras/hardware_params.hpp>

#include <span>
#include <vector>

namespace voxen::svc::detail
{

class TaskCounterTracker {
public:
	TaskCounterTracker() = default;
	TaskCounterTracker(TaskCounterTracker &&) = delete;
	TaskCounterTracker(const TaskCounterTracker &) = delete;
	TaskCounterTracker &operator=(TaskCounterTracker &&) = delete;
	TaskCounterTracker &operator=(const TaskCounterTracker &) = delete;
	~TaskCounterTracker() = default;

	uint64_t allocateCounter() noexcept;
	void completeCounter(uint64_t value);

	bool isCounterComplete(uint64_t counter) noexcept;
	size_t trimCompleteCounters(std::span<uint64_t> counters) noexcept;

private:
	constexpr static size_t NUM_COMPLETION_LISTS = 64;

	// Both ends inclusive: [first, last]
	using ValueSegment = std::pair<uint64_t, uint64_t>;

	struct alignas(extras::hardware_params::cache_line) CompletionList {
		std::atomic_uint64_t fully_completed_value = 0;
		std::vector<ValueSegment> out_of_order_segments;
		os::FutexLock lock;
	};

	alignas(extras::hardware_params::cache_line) std::atomic_uint64_t m_next_allocated_counter = NUM_COMPLETION_LISTS;
	CompletionList m_completion_lists[NUM_COMPLETION_LISTS];
};

} // namespace voxen::svc::detail
