#include <extras/futex.hpp>

#include <cassert>
#include <version>

// System-dependent includes
#include <emmintrin.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace extras
{

static_assert(std::atomic_uint32_t::is_always_lock_free, "Futex payload type must be lock-free");
// Might avoid cast if using `std::atomic_ref`, but it's not supported as of libc++ 13
static_assert(sizeof(std::atomic_uint32_t) == sizeof(uint32_t) && alignof(std::atomic_uint32_t) == alignof(uint32_t),
	"atomic_uint32_t must be interconvertible with uint32_t");

#if defined(__cpp_lib_atomic_ref) && __cpp_lib_atomic_ref >= 201806L
	#error 'std::atomic_ref' has arrived, rewrite this code
#endif

void futex::lock() noexcept
{
	constexpr uint32_t MAX_SPIN_COUNT = 16;

	uint32_t expected = 0;
	uint32_t spin_count = 0;

	do {
		expected = 0;
		// Optimistically assume lock is free (mostly true for short-time, low-contention locks)
		bool success = m_payload.compare_exchange_weak(expected, 1, std::memory_order_acquire, std::memory_order_relaxed);
		if (success) [[likely]] {
			return;
		}

		// We are the first to arrive at taken lock, spin for a short time in case it unlocks soon
		_mm_pause();
		spin_count++;
	} while (expected == 1 && spin_count < MAX_SPIN_COUNT);

	// Spinning wait failed, we have to use futex
	do {
		// Store 2 in the payload to mark there is at least one thread waiting on it
		if (expected == 2 || m_payload.compare_exchange_weak(expected, 2, std::memory_order_relaxed)) [[likely]] {
			// We are not the first to wait for this lock, go straight to waiting
			long r = syscall(SYS_futex, reinterpret_cast<uint32_t *>(&m_payload), FUTEX_WAIT_PRIVATE, 2, nullptr, nullptr,
				0);
			if (r == -1) {
				// Are other errors even possible?
				assert(errno == EAGAIN || errno == EINTR);
			}
		}

		expected = 0;
		// Here we try to store 2 as at this point we don't know
		// if there is other waiting thread, so play safe
	} while (!m_payload.compare_exchange_weak(expected, 2, std::memory_order_acquire, std::memory_order_relaxed));
}

bool futex::try_lock() noexcept
{
	uint32_t expected = 0;
	return m_payload.compare_exchange_weak(expected, 1, std::memory_order_acquire, std::memory_order_relaxed);
}

void futex::unlock() noexcept
{
	if (m_payload.fetch_sub(1, std::memory_order_release) != 1) {
		// At least one other thread is waiting on this lock, wake someone up
		m_payload.store(0, std::memory_order_release);

		[[maybe_unused]] long r = syscall(SYS_futex, reinterpret_cast<uint32_t *>(&m_payload), FUTEX_WAKE_PRIVATE, 1,
			nullptr, nullptr, 0);
		// Can FUTEX_WAKE even fail?
		assert(r != -1);
	}
}

} // namespace extras
