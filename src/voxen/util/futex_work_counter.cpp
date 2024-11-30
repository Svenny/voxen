#include <voxen/util/futex_work_counter.hpp>

#include <voxen/os/futex.hpp>

namespace voxen
{

namespace
{

constexpr uint32_t STOP_BIT = 1u << 31u;

FutexWorkCounter::Value unpackValue(uint32_t value) noexcept
{
	return { value & (STOP_BIT - 1u), !!(value & STOP_BIT) };
}

} // namespace

auto FutexWorkCounter::loadRelaxed() const noexcept -> Value
{
	return unpackValue(m_counter.load(std::memory_order_relaxed));
}

void FutexWorkCounter::addWork(uint32_t amount) noexcept
{
	if (m_counter.fetch_add(amount, std::memory_order_release) == 0) {
		os::Futex::wakeAll(&m_counter);
	}
}

auto FutexWorkCounter::removeWork(uint32_t amount) noexcept -> Value
{
	auto value = unpackValue(m_counter.fetch_sub(amount, std::memory_order_acq_rel));
	value.first -= amount;
	return value;
}

void FutexWorkCounter::requestStop() noexcept
{
	if (m_counter.fetch_or(STOP_BIT, std::memory_order_release) == 0) {
		os::Futex::wakeAll(&m_counter);
	}
}

auto FutexWorkCounter::wait() noexcept -> Value
{
	uint32_t value = m_counter.load(std::memory_order_acquire);

	while (value == 0) {
		os::Futex::waitInfinite(&m_counter, value);
		value = m_counter.load(std::memory_order_acquire);
	}

	return unpackValue(value);
}

} // namespace voxen
