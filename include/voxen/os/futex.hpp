#pragma once

#include <atomic>

namespace voxen::os
{

// Futex ops freely available to build custom synchronization primitives
namespace Futex
{

// Wait (block thread) until value at `addr` changes to something
// other than `value`. Might also return due to spurious wakeup. This function
// does not perform any checking loops, you are expected to do it yourself.
// This is a syscall so it's not particularly fast.
void waitInfinite(std::atomic_uint32_t *addr, uint32_t value) noexcept;

// Wake at least one thread (if any) waiting on `addr`.
// This is a syscall so it's not particularly fast.
void wakeSingle(std::atomic_uint32_t *addr) noexcept;

} // namespace Futex

// A lightweight alternative to `std::mutex` (4 bytes versus ~40-80).
// Trivial to create/destroy, can be used for fine-grained locks.
class FutexLock {
public:
	FutexLock() = default;
	FutexLock(FutexLock &&) = delete;
	FutexLock(const FutexLock &) = delete;
	FutexLock &operator=(FutexLock &&) = delete;
	FutexLock &operator=(const FutexLock &) = delete;
	~FutexLock() = default;

	void lock() noexcept;
	bool try_lock() noexcept;
	void unlock() noexcept;

private:
	std::atomic_uint32_t m_payload = 0;
};

} // namespace voxen::os
