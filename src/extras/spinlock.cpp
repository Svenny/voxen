#include <extras/spinlock.hpp>

#include <emmintrin.h>

#include <thread>

namespace extras
{

void spinlock::lock() noexcept
{
	constexpr int MAX_LOAD_LOOP_LENGTH = 64;

	// Optimistically assume lock is free (mostly true for short-time, low-contention locks)
	while (m_payload.exchange(true, std::memory_order_acquire) == true) {
		// Someone is in critical section now, don't make further attempts
		// to take a lock for a short period (to reduce cache coherency traffic)

		bool freed = false;
		for (int i = 0; i < MAX_LOAD_LOOP_LENGTH; i++) {
			_mm_pause();
			if (m_payload.load(std::memory_order_relaxed) == false) {
				// The lock got freed, we may try `exchange` again
				freed = true;
				break;
			}
		}

		if (!freed) {
			// The lock is being held for too long, fall back to "sleep"
			std::this_thread::yield();
		}
	}
}

bool spinlock::try_lock() noexcept
{
	return m_payload.exchange(true, std::memory_order_acquire) == false;
}

void spinlock::unlock() noexcept
{
	m_payload.store(false, std::memory_order_release);
}

}
