#pragma once

#include <voxen/svc/svc_fwd.hpp>
#include <voxen/visibility.hpp>

#include <cstdint>
#include <utility>

namespace voxen::svc
{

class VOXEN_API TaskHandle {
public:
	TaskHandle() = default;
	// Conversion from an internal handle type
	TaskHandle(detail::PrivateTaskHandle &&handle) noexcept;
	TaskHandle(TaskHandle &&other) noexcept : m_header(std::exchange(other.m_header, nullptr)) {}
	TaskHandle(const TaskHandle &other) noexcept;
	TaskHandle &operator=(TaskHandle &&other) noexcept
	{
		std::swap(m_header, other.m_header);
		return *this;
	}
	TaskHandle &operator=(const TaskHandle &other) noexcept;
	~TaskHandle();

	// Reset ownership, decreasing its refcount and potentially deallocating.
	// After calling this function `valid()` will return false.
	void reset() noexcept;

	// Non-blocking check if this task has finished executing.
	// Behavior is undefined if `valid() == false`.
	bool finished() const noexcept;
	// Block until task execution completes, i.e. `finished()` becomes true.
	// Behavior is undefined if `valid() == false`.
	void wait() noexcept;

	// Check if this handle owns a valid task
	bool valid() const noexcept { return m_header != nullptr; }

	// Wait counter of this task, zero if `valid() == false`
	uint64_t getCounter() const noexcept;

protected:
	detail::TaskHeader *m_header = nullptr;

	void addRef() noexcept;
};

} // namespace voxen::svc
