#pragma once

#include <voxen/svc/svc_fwd.hpp>
#include <voxen/svc/task_handle.hpp>
#include <voxen/visibility.hpp>

#include <extras/pimpl.hpp>

#include <cstdint>
#include <span>

namespace voxen::svc
{

// Task callable object must have function-like signature `task(TaskContext &ctx)`
template<typename T>
concept CTaskObject = std::is_nothrow_destructible_v<T> && std::is_invocable_v<T, TaskContext &>;

// Provides interface to setup and enqueue tasks for asynchronous execution.
// This class is intended to be used within the scope of a single function.
class VOXEN_API TaskBuilder {
public:
	// Create a builder not attached to a specific task context.
	// Unless you manually set up dependencies with `addWait()`, its enqueued tasks
	// are not dependent on anything and will begin executing as soon as possible.
	explicit TaskBuilder(TaskService &svc);
	// Create a builder attached to a context of executing task.
	// This mode has a different synchronization behavior - tasks enqueued
	// from this builder are continuations of the original task from `ctx`.
	//
	// Continuations will begin executing only when the original task functor ends,
	// in addition to any manually provided dependency. Also, the original task's
	// counter will be considered completed only when every continuation task ends,
	// including all the recursively launched continuations (the whole task tree).
	//
	// Calling `addWait()` with the current task counter WILL deadlock the program.
	// Same applies to the counter of any subtask in the tree. In general,
	// you should avoid adding any dependencies in continuation chain at all.
	//
	// To break the continuation chain and enqueue independent tasks, create builder
	// from a task service reference: `TaskBuilder bld(ctx.taskService());`
	//
	// One more note: continuation chain can be actually walked recursively inside,
	// and it will be at least during internal resource cleanup. So making excessively
	// deep continuation chains has the risk of stack overflow as well. In general,
	// you should avoid making continuation chains more than 1-2 levels deep.
	explicit TaskBuilder(TaskContext &ctx);
	TaskBuilder(TaskBuilder &&) = delete;
	TaskBuilder(const TaskBuilder &) = delete;
	TaskBuilder &operator=(TaskBuilder &&) = delete;
	TaskBuilder &operator=(const TaskBuilder &) = delete;
	~TaskBuilder();

	// The next enqueued task will wait for `counter` to finish before it can start executing.
	// `counter` must be returned from a previous call to `getLastTaskCounter()`,
	// not necessarily from the same builder instance.
	//
	// If multiple counters are added by successive calls of
	// this function, the task will wait for all of them to finish.
	//
	// After enqueueing a task the set of wait counters is reset,
	// meaning the next task will NOT wait on these values too.
	void addWait(uint64_t counter);
	// Behaves exactly as if a single-value `addWait()` is called for every value
	void addWait(std::span<const uint64_t> counters);

	// Enqueue task defined by functor (callable object).
	// There is no way to retrieve `TaskHandle` for it.
	template<CTaskObject T>
	void enqueueTask(T &&task)
	{
		createTask(std::forward<T>(task));
		doEnqueueTask();
	}

	// Enqueue a task defined by functor (callable object)
	// and return a `TaskHandle` pointing to it.
	template<CTaskObject T>
	TaskHandle enqueueTaskWithHandle(T &&task)
	{
		createTask(std::forward<T>(task));
		return doEnqueueTaskWithHandle();
	}

	// Conceptually this is equal to `enqueueTaskWithHandle(<empty lambda>)`.
	// An idiomatic way to get a "group" handle to wait for a set of tasks (`addWait()`).
	TaskHandle enqueueSyncPoint();

	// Return a waitable counter assigned to the last task enqueued from this builder.
	// If no tasks were enqueued returns zero, which is considered initially complete and valid to "wait" on.
	// This value can be passed to `addWait()` of this or any other `TaskBuilder` instance,
	// allowing you to dynamically create task dependency graphs of arbitrary complexity.
	uint64_t getLastTaskCounter() const noexcept;

private:
	struct Impl;
	extras::pimpl<Impl, 64, 8> m_impl;

	struct TaskPayloadInfo {
		void (*call)(void *functor_storage, TaskContext &ctx);
		void (*dtor)(void *functor_storage) noexcept;
	};

	template<CTaskObject T>
	void createTask(T &&task)
	{
		using FT = std::remove_cvref_t<T>;
		static_assert(alignof(FT) <= alignof(uint64_t), "Task functor is over-aligned");

		void *functor_storage = createTaskHandle(sizeof(FT));
		new (functor_storage) FT(std::forward<FT>(task));

		setTaskPayload({
			.call = [](void *ptr, TaskContext &ctx) { reinterpret_cast<FT *>(ptr)->operator()(ctx); },
			.dtor = [](void *ptr) noexcept { reinterpret_cast<FT *>(ptr)->~FT(); },
		});
	}

	void *createTaskHandle(size_t functor_size);
	void setTaskPayload(TaskPayloadInfo info);
	void doEnqueueTask();
	TaskHandle doEnqueueTaskWithHandle();
};

} // namespace voxen::svc
