#pragma once

#include <atomic>

namespace extras
{

// A lightweight alternative to `std::mutex` (4 bytes versus 40)
class futex final {
public:
	futex() = default;
	futex(futex &&) = delete;
	futex(const futex &) = delete;
	futex &operator = (futex &&) = delete;
	futex &operator = (const futex &) = delete;
	~futex() = default;

	void lock() noexcept;
	bool try_lock() noexcept;
	void unlock() noexcept;

private:
	std::atomic_uint32_t m_payload = 0;
};

}
