#pragma once

#include <atomic>
#include <utility>

namespace voxen
{

// A tiny utility to help threads wait for incoming work without busy looping.
// Stores combined outstanding work counter and "stop requested" flag.
// Even though it handles all syscalls and atomic operations internally
// you are still responsible for using its functions correctly.
class FutexWorkCounter final {
public:
	// <remaining work items; stop flag>
	using Value = std::pair<uint32_t, bool>;

	FutexWorkCounter() = default;
	FutexWorkCounter(FutexWorkCounter &&) = delete;
	FutexWorkCounter(const FutexWorkCounter &) = delete;
	FutexWorkCounter &operator=(FutexWorkCounter &&) = delete;
	FutexWorkCounter &operator=(const FutexWorkCounter &) = delete;
	~FutexWorkCounter() = default;

	// Get current value with `std::memory_order_relaxed`.
	// Can be used from any thread. You can use this value merely as a hint
	// but not for synchronization. For example, you might select a slave
	// thread with the lowest amount of outstanding work using it.
	Value loadRelaxed() const noexcept;

	// Increase outstanding work counter by `amount`
	// and wake all threads waiting on it
	void addWork(uint32_t amount) noexcept;
	// Decrease outstanding work counter by `amount`,
	// acknowledging that these work items have been consumed.
	// Returns the counter value *after* decreasing.
	Value removeWork(uint32_t amount) noexcept;
	// Raise the stop flag and wake any waiting thread
	void requestStop() noexcept;

	// Block until either some work is added (so that the counter becomes non-zero)
	// or the stop flag is raised. One counter can be waited on by mutliple threads.
	// Returns the counter value after waiting.
	Value wait() noexcept;

private:
	std::atomic_uint32_t m_counter = 0;
};

} // namespace voxen
