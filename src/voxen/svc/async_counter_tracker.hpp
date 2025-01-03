#pragma once

#include <voxen/os/futex.hpp>
#include <voxen/svc/service_base.hpp>

#include <extras/hardware_params.hpp>

#include <span>
#include <vector>

namespace voxen::svc::detail
{

// This is an internal service managing completion/waitable counters for
// all kinds of asynchronous operations in CPU domain. These can include
// compute tasks as well as disk/network/etc. IO. This is not used for GPU
// synchronization but can be used e.g. to wait for GPU offloads on CPU.
//
// Counters are used to both check for completions and to express
// dependencies between different operations in a unified way.
// They are also essentially "weak handles", meaning allocated counters
// can be stored forever, and completed counters don't consume any memory.
//
// Not exposed outside as this service is quite dangerous if misused.
// It should be called only inside higher-level asynchronous services.
// Pretty much like with `PipeMemoryAllocator`, expect increased memory
// usage and a growing performance hit if even one allocated counter
// is not marked as complete in a reasonable amount of time.
class AsyncCounterTracker final : public IService {
public:
	constexpr static UID SERVICE_UID = UID("95179c38-a5be89ed-c2be9d72-c8ce8057");

	AsyncCounterTracker() = default;
	AsyncCounterTracker(AsyncCounterTracker &&) = delete;
	AsyncCounterTracker(const AsyncCounterTracker &) = delete;
	AsyncCounterTracker &operator=(AsyncCounterTracker &&) = delete;
	AsyncCounterTracker &operator=(const AsyncCounterTracker &) = delete;
	~AsyncCounterTracker() override;

	UID serviceUid() const noexcept override { return SERVICE_UID; }

	// Allocate a new counter value. It is considered incomplete
	// and *MUST* be completed later with `completeCounter()` call.
	// Returned value is strictly larger than any previously allocated one.
	uint64_t allocateCounter();

	// Mark value previously returned from `allocateCounter()` as complete.
	// This function must be called *exactly once* for any given value.
	void completeCounter(uint64_t value);

	// Check if a given counter is marked as complete.
	// Zero is considered always complete.
	bool isCounterComplete(uint64_t counter) noexcept;

	// Check a set of counters for completion and remove completed ones.
	// Returns the number of remaining incomplete counters - they will be moved
	// to the first consecutive elements of `counters` in unspecified order.
	// The remaining elements of `counters` will have undefined (garbage) values.
	size_t trimCompleteCounters(std::span<uint64_t> counters) noexcept;

private:
	// Multiple "completion lists" are used to spread thread contention.
	// The list corresponding to a given value is selected with modulo
	// operation, so this value should ideally be a constant power of two.
	constexpr static size_t NUM_COMPLETION_LISTS = 64;

	// Both ends inclusive: [first, last]
	using ValueSegment = std::pair<uint64_t, uint64_t>;

	// Completion list stores counter values divided by `NUM_COMPLETION_LIST`
	// so that they form a continuous sequence 0, 1, 2, ... inside the list
	struct alignas(extras::hardware_params::cache_line) CompletionList {
		// This and every smaller value is completed
		std::atomic_uint64_t fully_completed_value = 0;
		// Segments of completed values with some gap from `fully_completed_value`.
		// They cannot overlap and are always kept sorted in descending order.
		std::vector<ValueSegment> out_of_order_segments;
		os::FutexLock lock;
	};

	// Initial value is `NUM_COMPLETION_LISTS`, it gives 1 in every list after division
	alignas(extras::hardware_params::cache_line) std::atomic_uint64_t m_next_allocated_counter = NUM_COMPLETION_LISTS;
	CompletionList m_completion_lists[NUM_COMPLETION_LISTS];
};

} // namespace voxen::svc::detail
