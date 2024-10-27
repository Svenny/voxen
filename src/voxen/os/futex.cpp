#include <voxen/os/futex.hpp>

#include <cassert>

// TODO: x86-specific include... but what, are we going to support arm?
#include <emmintrin.h>

#ifndef _WIN32
	#include <linux/futex.h>
	#include <sys/syscall.h>
	#include <unistd.h>
#else
	#include <Windows.h>
	// --- dont reorder
	#include <synchapi.h>
#endif

namespace voxen::os
{

#ifndef _WIN32

void Futex::waitInfinite(std::atomic_uint32_t *addr, uint32_t value) noexcept
{
	long res = syscall(SYS_futex, addr, FUTEX_WAIT_PRIVATE, value, nullptr, nullptr, 0);
	if (res == -1) {
		// These codes are not harmful; are other even possible?
		assert(errno == EAGAIN || errno == EINTR);
	}
}

void Futex::wakeSingle(std::atomic_uint32_t *addr) noexcept
{
	[[maybe_unused]] long res = syscall(SYS_futex, addr, FUTEX_WAKE_PRIVATE, 1, nullptr, nullptr, 0);
	// Nothing to fail here
	assert(res != -1);
}

void Futex::wakeAll(std::atomic_uint32_t *addr) noexcept
{
	[[maybe_unused]] long res = syscall(SYS_futex, addr, FUTEX_WAKE_PRIVATE, INT32_MAX, nullptr, nullptr, 0);
	// Nothing to fail here
	assert(res != -1);
}

#else // _WIN32

void Futex::waitInfinite(std::atomic_uint32_t *addr, uint32_t value) noexcept
{
	[[maybe_unused]] BOOL res = WaitOnAddress(addr, &value, 4, INFINITE);
	// Nothing should fail here
	assert(res == TRUE);
}

void Futex::wakeSingle(std::atomic_uint32_t *addr) noexcept
{
	WakeByAddressSingle(addr);
}

void Futex::wakeAll(std::atomic_uint32_t *addr) noexcept
{
	WakeByAddressAll(addr);
}

#endif // _WIN32

// --- FutexLock ---

void FutexLock::lock() noexcept
{
	constexpr uint32_t MAX_SPIN_COUNT = 4;

	uint32_t expected = 0;
	uint32_t spin_count = 0;

	// TODO: this part was not thoroughly analyzed nor tuned for modern x86 CPUs.
	// It should be relatively short and have little effect, though.
	while (spin_count < MAX_SPIN_COUNT) {
		// Optimistically assume lock is free (mostly true for short-time, low-contention locks)
		expected = 0;
		bool success = m_payload.compare_exchange_weak(expected, 1, std::memory_order_acquire, std::memory_order_relaxed);
		if (success) [[likely]] {
			// The lock is ours!
			return;
		}

		if (expected == 2) [[unlikely]] {
			// Someone is already waiting, skip all the spinning part and go straight to waiting
			break;
		}

		// We are the first to arrive at taken lock, spin for a short time in case it unlocks soon.
		// Use small exponentially growing number of spins. Note that `pause` has wildly varying
		// latency on different CPUs, this place is just impossible to properly tune for all of them.
		for (uint32_t i = 0; i < (1u << spin_count); i++) {
			_mm_pause();
		}

		spin_count++;
	}

	// Spinning wait failed, we have to ask the kernel for help
	do {
		// Here `expected` can be 1 or 2 (we'd already taken the lock if it was 0).
		// Store 2 (if not already) to mark there is a thread waiting on the lock.
		// CAS(1, 2) might fail - no problem, we will just try again in the next iteration.
		if (expected == 2 || m_payload.compare_exchange_weak(expected, 2, std::memory_order_relaxed)) [[likely]] {
			// Wait until it becomes something other than 2
			Futex::waitInfinite(&m_payload, 2);
		}

		expected = 0;
		// Try acquiring the lock after wakeup.
		// We don't know if there are any other waiting threads,
		// so we have to be conservative and write 2 instead of 1.
	} while (!m_payload.compare_exchange_weak(expected, 2, std::memory_order_acquire, std::memory_order_relaxed));
}

bool FutexLock::try_lock() noexcept
{
	uint32_t expected = 0;
	return m_payload.compare_exchange_strong(expected, 1, std::memory_order_acquire, std::memory_order_relaxed);
}

void FutexLock::unlock() noexcept
{
	if (m_payload.exchange(0, std::memory_order_release) == 2) {
		// Someone is waiting on this lock, wake him up
		Futex::wakeSingle(&m_payload);
	}
}

// --- FutexRWLock ---

constexpr static uint32_t RWLOCK_EXCLUSIVE_BIT = 1u << 31;
constexpr static uint32_t RWLOCK_WAITING_BIT = 1u << 30;

void FutexRWLock::lock() noexcept
{
	// Optimistically assume the lock is free
	uint32_t expected = 0;
	if (m_payload.compare_exchange_strong(expected, RWLOCK_EXCLUSIVE_BIT, std::memory_order_acquire,
			 std::memory_order_relaxed)) [[likely]] {
		// The lock is ours!
		return;
	}

	// TODO: we could spin a bit (see `FutexLock::lock()`) assuming the lock will get released soon

	do {
		// We might have either some shared locks or an exclusive one.
		// In either case try to insert the waiting bit and then, well, wait.
		// CAS might fail - no problem, we will just try again in the next iteration.
		uint32_t desired = expected | RWLOCK_WAITING_BIT;
		if (expected == desired || m_payload.compare_exchange_weak(expected, desired, std::memory_order_relaxed))
			[[likely]] {
			// Wait until it gets some other value
			Futex::waitInfinite(&m_payload, desired);
		}

		expected = 0;
		// Try acquiring the lock after wakeup.
		// We don't know if there are any other waiting threads,
		// so we have to be conservative and retain the waiting bit.
	} while (!m_payload.compare_exchange_weak(expected, RWLOCK_EXCLUSIVE_BIT | RWLOCK_WAITING_BIT,
		std::memory_order_acquire, std::memory_order_relaxed));
}

bool FutexRWLock::try_lock() noexcept
{
	uint32_t expected = 0;
	return m_payload.compare_exchange_strong(expected, RWLOCK_EXCLUSIVE_BIT, std::memory_order_acquire,
		std::memory_order_relaxed);
}

void FutexRWLock::unlock() noexcept
{
	if (m_payload.exchange(0, std::memory_order_release) & RWLOCK_WAITING_BIT) {
		// Someone is waiting on this lock.
		// *All* shared lock attempts should be woken up here,
		// there will be no other place where we can do it.
		// If those are exclusive ones... well, not enough information to discern.
		Futex::wakeAll(&m_payload);
	}
}

void FutexRWLock::lock_shared() noexcept
{
	while (true) {
		uint32_t expected = m_payload.load(std::memory_order_relaxed);

		// Assume there is no exclusive lock (waiting bit doesn't matter)
		// Loop because CAS might fail - this might be only because someone
		// else is taking a shared lock too or setting a waiting bit.
		while (!(expected & RWLOCK_EXCLUSIVE_BIT)) [[likely]] {
			if (m_payload.compare_exchange_weak(expected, expected + 1, std::memory_order_acquire,
					 std::memory_order_relaxed)) [[likely]] {
				// Welcome to the club, buddy!
				return;
			}

			// Just slightly relax CPU before trying again
			_mm_pause();
		}

		// TODO: we could spin a bit (see `FutexLock::lock()`) assuming the lock will get released soon

		// There is an exclusive lock held.
		// Try to insert the waiting bit and then, well, wait.
		// CAS might fail - no problem, we will just try again in the next iteration.
		uint32_t desired = expected | RWLOCK_WAITING_BIT;
		if (expected == desired || m_payload.compare_exchange_weak(expected, desired, std::memory_order_relaxed))
			[[likely]] {
			// Wait until it gets some other value
			Futex::waitInfinite(&m_payload, desired);
		}
	}
}

bool FutexRWLock::try_lock_shared() noexcept
{
	uint32_t expected = m_payload.load(std::memory_order_relaxed);
	if (expected & RWLOCK_EXCLUSIVE_BIT) [[unlikely]] {
		return false;
	}

	// Not in exclusive lock state - try adding one more shared lock user
	return m_payload.compare_exchange_strong(expected, expected + 1, std::memory_order_acquire,
		std::memory_order_relaxed);
}

void FutexRWLock::unlock_shared() noexcept
{
	// Remove one shared lock user
	if (m_payload.fetch_sub(1, std::memory_order_release) & RWLOCK_WAITING_BIT) {
		// Someone is waiting on this lock, wake him up.
		// It can only be an exclusive lock attempt (shared wouldn't wait).
		m_payload.fetch_and(~RWLOCK_WAITING_BIT, std::memory_order_release);
		Futex::wakeSingle(&m_payload);
	}
}

} // namespace voxen::os
