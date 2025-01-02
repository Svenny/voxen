#pragma once

#include <voxen/svc/pipe_memory_function.hpp>
#include <voxen/svc/task_coro.hpp>
#include <voxen/svc/task_handle.hpp>

#include <atomic>
#include <cstdint>

namespace voxen::svc::detail
{

class ParentTaskHandle {
public:
	ParentTaskHandle() = default;
	ParentTaskHandle(ParentTaskHandle &&) = delete;
	ParentTaskHandle(const ParentTaskHandle &) = delete;
	ParentTaskHandle &operator=(ParentTaskHandle &&) = delete;
	ParentTaskHandle &operator=(const ParentTaskHandle &) = delete;
	~ParentTaskHandle();

	void setParent(TaskHeader *header) noexcept;
	void onTaskComplete(TaskCounterTracker &tracker);

private:
	TaskHeader *m_parent = nullptr;
};

// Stores task control information and payload.
// For refcount safety access it through `TaskHandle`.
//
// Current implementation stores two variable-sized items
// in the same pipe memory allocation after the struct:
// - Wait counters array, immediately after the struct
// - Functor storage bytes for lambda captures etc. + possible alignment
struct TaskHeader {
	constexpr static uint32_t MAX_WAIT_COUNTERS = (1u << 31) - 1u;

	// Atomic value for per-task locking, status, refcounting etc.
	// In current implementation, stores:
	// Bits [15:0] - refcount (initially 1 from header pointer after allocation)
	// Bits [16:16] - futex completion waiting flag (0 - no waiting, 1 - needs waking)
	// Bits [17:17] - completion status (0 - pending, 1 - finished)
	// Bits [19:18] - unused, must be zero
	// Bits [31:20] - continuation count (number of pending tasks for which this is a parent)
	std::atomic_uint32_t atomic_word = 1;
	// Number of valid counter values in `waitCountersArray()`.
	// When it reaches zero, the task becomes ready to execute.
	uint32_t num_wait_counters : 31 = 0;
	// Whether `function` or `coroutine` is the active member of `executable`
	uint32_t stores_coroutine : 1 = 0;

	// Executable object, either function or coroutine (can be none for "sync point" tasks).
	// Note: initialized with empty function (conforms to `stores_coroutine = 0` initializer).
	union ExecutableUnion {
		PipeMemoryFunction<void(TaskContext &ctx)> function = {};
		CoroTask coroutine;

		ExecutableUnion() noexcept {}
		~ExecutableUnion() noexcept {}
	} executable;

	// Special handle to the parent task, if not null then this task is its continuation.
	// Completion signal must be propagated to it through `PrivateTaskHandle::complete()`.
	// Destroying the object with active (uncompleted) parent reference will make
	// parent's counter never complete, and will then blow up the whole task system.
	ParentTaskHandle parent_handle;
	// Counter value associated with this task.
	// Note - the last field of this struct aligns the immediately following wait counters array.
	uint64_t task_counter = 0;

	~TaskHeader() noexcept
	{
		if (stores_coroutine) {
			executable.coroutine.~CoroTask();
		} else {
			executable.function.~PipeMemoryFunction();
		}
	}

	uint64_t *waitCountersArray() noexcept { return reinterpret_cast<uint64_t *>(this + 1); }
};

// Internal extended version of `TaskHandle`
class PrivateTaskHandle : public TaskHandle {
public:
	PrivateTaskHandle() = default;
	// Assumes ownership of a raw pointer without incrementing refcount
	explicit PrivateTaskHandle(detail::TaskHeader *header) noexcept { m_header = header; }
	// Conversion from a public handle
	PrivateTaskHandle(TaskHandle &&handle) noexcept : TaskHandle(std::move(handle)) {}
	PrivateTaskHandle(PrivateTaskHandle &&) = default;
	PrivateTaskHandle(const PrivateTaskHandle &) = default;
	PrivateTaskHandle &operator=(PrivateTaskHandle &&) = default;
	PrivateTaskHandle &operator=(const PrivateTaskHandle &) = default;
	~PrivateTaskHandle() = default;

	bool hasContinuations() const noexcept;

	// Mark this task as finished and wake all threads possibly waiting on it.
	// If task execution was started (its functor was called), this function
	// MUST be called, otherwise task continuation system will blow up.
	//
	// Task ownership is released before signaling completion. This is needed when this handle
	// holds the only live reference, then resources associated with the task will be freed
	// before its completion is acknowledged. Needed e.g. for subsystem destructors waiting
	// for all enqueued tasks completion.
	void completeAndReset(TaskCounterTracker &tracker);

	// Get raw pointer without affecting ownership
	TaskHeader *get() noexcept { return m_header; }
	// Release ownership of a pointer without decrementing refcount
	[[nodiscard]] TaskHeader *release() noexcept { return std::exchange(m_header, nullptr); }
};

} // namespace voxen::svc::detail
