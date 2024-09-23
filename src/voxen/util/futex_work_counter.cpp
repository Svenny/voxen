#include <voxen/util/futex_work_counter.hpp>

#include <cassert>

// TODO: system-dependent includes; wrap in conditional
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace voxen
{

namespace
{

constexpr uint32_t STOP_BIT = 1u << 31u;

FutexWorkCounter::Value unpackValue(uint32_t value) noexcept
{
	return { value & (STOP_BIT - 1u), !!(value & STOP_BIT) };
}

void futexWake(std::atomic_uint32_t *counter) noexcept
{
	[[maybe_unused]] long r = syscall(SYS_futex, reinterpret_cast<uint32_t *>(counter), FUTEX_WAKE_PRIVATE, 1, nullptr,
		nullptr, 0);
	// Can FUTEX_WAKE even fail?
	assert(r != -1);
}

void futexWaitNonzero(std::atomic_uint32_t *counter) noexcept
{
	long r = syscall(SYS_futex, reinterpret_cast<uint32_t *>(counter), FUTEX_WAIT_PRIVATE, 0, nullptr, nullptr, 0);
	if (r == -1) {
		// Are other errors even possible?
		assert(errno == EAGAIN || errno == EINTR);
	}
}

} // namespace

auto FutexWorkCounter::loadRelaxed() const noexcept -> Value
{
	return unpackValue(m_counter.load(std::memory_order_relaxed));
}

void FutexWorkCounter::addWork(uint32_t amount) noexcept
{
	if (m_counter.fetch_add(amount) == 0) {
		futexWake(&m_counter);
	}
}

auto FutexWorkCounter::removeWork(uint32_t amount) noexcept -> Value
{
	auto value = unpackValue(m_counter.fetch_sub(amount));
	value.first -= amount;
	return value;
}

void FutexWorkCounter::requestStop() noexcept
{
	if (m_counter.fetch_or(STOP_BIT) == 0) {
		futexWake(&m_counter);
	}
}

auto FutexWorkCounter::wait() noexcept -> Value
{
	uint32_t value = m_counter.load();

	while (value == 0) {
		futexWaitNonzero(&m_counter);
		value = m_counter.load();
	}

	return unpackValue(value);
}

} // namespace voxen
