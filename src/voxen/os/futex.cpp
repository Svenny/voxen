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

#endif // _WIN32

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
		// Try acquiring the lock after wakeup
	} while (!m_payload.compare_exchange_weak(expected, 1, std::memory_order_acquire, std::memory_order_relaxed));
}

bool FutexLock::try_lock() noexcept
{
	uint32_t expected = 0;
	return m_payload.compare_exchange_strong(expected, 1, std::memory_order_acquire, std::memory_order_relaxed);
}

void FutexLock::unlock() noexcept
{
	if (m_payload.fetch_sub(1, std::memory_order_release) == 2) {
		// Someone is waiting on this lock, wake him up
		m_payload.store(0, std::memory_order_release);
		Futex::wakeSingle(&m_payload);
	}
}

} // namespace voxen::os
