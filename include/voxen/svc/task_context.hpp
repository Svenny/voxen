#pragma once

#include <voxen/svc/svc_fwd.hpp>
#include <voxen/visibility.hpp>

#include <cstdint>

namespace voxen::svc
{

// A temporary entity passed to executing task functors.
// This class can be used only within the scope of a task functor.
class VOXEN_API TaskContext {
public:
	// This is an internal constructor, you cannot instantiate this object directly
	explicit TaskContext(TaskService &service, detail::PrivateTaskHandle &handle) noexcept
		: m_service(service), m_handle(handle)
	{}
	TaskContext(TaskContext &&) = delete;
	TaskContext(const TaskContext &) = delete;
	TaskContext &operator=(TaskContext &&) = delete;
	TaskContext &operator=(const TaskContext &) = delete;
	~TaskContext() = default;

	// Task service executing this. You can create `TaskBuilder`
	// from it to launch independent, non-continuation tasks.
	TaskService &taskService() noexcept { return m_service; }

	// Get waitable counter assigned to this task.
	// NOTE: DO NOT use it with `TaskBuilder::addWait()` if that builder
	// is created for this context, not for `TaskService`. In other words,
	// don't make continuation tasks wait on their parent.
	// This WILL deadlock the program.
	// Also, don't use it for any recursive continuations as well.
	uint64_t getThisTaskCounter() noexcept;

	// Get task header without adding a ref.
	// This is an internal method, it's not useful externally.
	detail::TaskHeader *getThisTaskHeader() noexcept;

private:
	TaskService &m_service;
	detail::PrivateTaskHandle &m_handle;
};

} // namespace voxen::svc
