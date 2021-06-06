#pragma once

#include <atomic>

namespace extras
{

// A lightweight alternative for `std::mutex`, may be used for protecting
// short critical sections with small amount of expected thread contention
class spinlock final {
public:
	spinlock() = default;
	spinlock(spinlock &&) = delete;
	spinlock(const spinlock &) = delete;
	spinlock &operator = (spinlock &&) = delete;
	spinlock &operator = (const spinlock &) = delete;
	~spinlock() = default;

	void lock() noexcept;
	bool try_lock() noexcept;
	void unlock() noexcept;

private:
	std::atomic_bool m_payload = false;
};

}
