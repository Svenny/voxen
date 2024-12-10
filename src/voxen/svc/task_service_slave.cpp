#include "task_service_slave.hpp"

#include <voxen/debug/thread_name.hpp>
#include <voxen/svc/task_context.hpp>
#include <voxen/svc/task_service.hpp>

#include "task_counter_tracker.hpp"
#include "task_handle_private.hpp"
#include "task_queue_set.hpp"

namespace voxen::svc::detail
{

namespace
{

struct SlaveState {
	TaskService &task_service;
	TaskCounterTracker &counter_tracker;
	TaskQueueSet &queue_set;

	// Store waiting tasks locally to not keep them in the limited ring buffer.
	// Unless we can somehow reschedule them in a cache-aware way, there is
	// not much sense moving them to any other thread.
	std::vector<PrivateTaskHandle> local_waiting_queue = {};
};

void executeAndResetTask(SlaveState &state, PrivateTaskHandle &task)
{
	TaskHeader *header = task.get();

	// Sync point tasks have no functor
	if (header->call_fn) [[likely]] {
		TaskContext ctx(state.task_service, task);
		// TODO: exception safety, wrap in try/catch and store the exception
		header->call_fn(header->functorStorage(), ctx);
	}

	// TODO: defer enqueueing continuations until this point.
	// There is a (very slight) chance that all of them finish before
	// this check, or between the check and `task.reset()` for `else` branch.
	// In the first case we will have double completion (from a continuation
	// and from here), in the second we will not release resources before task
	// counter is completed if this was the last ref - which might break the
	// behavior of e.g. system destructors waiting on task counters to finish.
	//
	// Both cases are errors, and both are solved if we simply enqueue continuations
	// after this check. Actually, deferring just one continuation launch is enough,
	// all others can be enqueued immediately during the task execution.
	if (!task.hasContinuations()) {
		// Signal task completion, otherwise some child will do it
		task.completeAndReset(state.counter_tracker);
	} else {
		task.reset();
	}
}

// Update wait status of all tasks in the local queue and execute them if possible.
// Removes executed task handles, does not change the order of the rest.
void tryDrainLocalQueue(SlaveState &state)
{
	size_t remaining_tasks = 0;

	for (size_t i = 0; i < state.local_waiting_queue.size(); i++) {
		TaskHeader *header = state.local_waiting_queue[i].get();

		size_t remaining_counters = state.counter_tracker.trimCompleteCounters(
			std::span(header->waitCountersArray(), header->num_wait_counters));
		header->num_wait_counters = static_cast<decltype(header->num_wait_counters)>(remaining_counters);

		if (remaining_counters == 0) {
			// Ready now, execute and reset it, leaving an empty spot in the vector
			executeAndResetTask(state, state.local_waiting_queue[i]);
		} else {
			// Still not ready, move this task into the first empty spot.
			// If no task was reset in the above branch yet, this will just swap with itself.
			std::swap(state.local_waiting_queue[i], state.local_waiting_queue[remaining_tasks]);
			remaining_tasks++;
		}
	}

	// Remove null handles from executed tasks
	state.local_waiting_queue.erase(state.local_waiting_queue.begin() + ptrdiff_t(remaining_tasks),
		state.local_waiting_queue.end());
}

} // namespace

void TaskServiceSlave::threadFn(TaskService &my_service, size_t my_queue, TaskCounterTracker &counter_tracker,
	TaskQueueSet &queue_set)
{
	debug::setThreadName("ThreadPool@%zu", my_queue);

	SlaveState state {
		.task_service = my_service,
		.counter_tracker = counter_tracker,
		.queue_set = queue_set,
	};

	PrivateTaskHandle task = queue_set.popTaskOrWait(my_queue);
	size_t executed_independent_tasks = 0;

	// When the queue returns null handle it means a stop flag was raised
	while (task.valid()) {
		TaskHeader *header = task.get();

		if (header->num_wait_counters > 0) {
			// This task might be not executable right away.
			// Put it in the local queue and immediately try draining it while retaining FIFO order.
			// Previous waiting tasks might be dependencies of this one. Hence trying to execute
			// them first makes sense - might immediately unblock some waiting tasks added later.
			state.local_waiting_queue.emplace_back(std::move(task));
			tryDrainLocalQueue(state);
		} else {
			// Task without dependencies, execute it right away
			executeAndResetTask(state, task);

			executed_independent_tasks++;
			// TODO: adaptive/configurable constant?
			if (!state.local_waiting_queue.empty() && executed_independent_tasks > 50) {
				// Avoid large runs of independent tasks without checking local queue
				tryDrainLocalQueue(state);
				executed_independent_tasks = 0;
			}
		}

		// Take the next task from the queue.
		// We can't call `popTaskOrWait` while we have any waiting tasks. It will
		// deadlock the system if these waiting tasks are themselves being waited on.
		if (!state.local_waiting_queue.empty()) {
			// Try taking the task without waiting
			task = queue_set.tryPopTask(my_queue);

			// If we've received a valid handle, then just continue the main loop
			// trying to execute it. Otherwise we know the input queue is empty and
			// we have nothing to do for a while - might go over waiting tasks
			// in the meantime and then try getting a handle again. Unless
			// the system is deadlocked, we are guaranteed to eventually drain
			// the waiting queue (in finite time) and exit this loop.
			while (!task.valid() && !state.local_waiting_queue.empty()) {
				tryDrainLocalQueue(state);
				task = queue_set.tryPopTask(my_queue);
			}

			// If the above loop stopped because the waiting queue got empty
			// but the task handle is still null, wait for it or the main loop
			// condition will confuse it with stop flag and exit the thread.
			if (!task.valid()) {
				task = queue_set.popTaskOrWait(my_queue);
			}
		} else {
			// Wait (sleep) until the next task comes in.
			// If this returns null handle then a stop flag was raised.
			task = queue_set.popTaskOrWait(my_queue);
		}
	}
}

} // namespace voxen::svc::detail
