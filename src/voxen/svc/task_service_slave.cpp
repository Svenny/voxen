#include "task_service_slave.hpp"

#include <voxen/debug/thread_name.hpp>
#include <voxen/svc/task_context.hpp>
#include <voxen/svc/task_service.hpp>

#include "async_counter_tracker.hpp"
#include "task_handle_private.hpp"
#include "task_queue_set.hpp"

namespace voxen::svc::detail
{

namespace
{

struct SlaveState {
	TaskService &task_service;
	AsyncCounterTracker &counter_tracker;
	TaskQueueSet &queue_set;

	// Store waiting tasks locally to not keep them in the limited ring buffer.
	// Unless we can somehow reschedule them in a cache-aware way, there is
	// not much sense moving them to any other thread.
	std::vector<PrivateTaskHandle> local_waiting_queue = {};
};

// Attempts to execute coroutine task, must not be "initially" blocked i.e. `num_wait_counters == 0`.
// Check if the coroutine await stack is dynamically blocked (on an external task counter),
// and if it's not, resumes coroutines until either one blocks again or all of them complete.
// Returns `true` when the task is finished and can be destroyed/completion signaled.
bool tryExecuteCoroutineTask(SlaveState &state, CoroTask::RawHandle coro)
{
	// Well, in theory user could enqueue null handle or a terminated coroutine... but what for?
	if (!coro || coro.done()) [[unlikely]] {
		return true;
	}

	CoroTaskState &coro_state = coro.promise();

	if (uint64_t counter = coro_state.blockedOnCounter(); counter > 0) {
		if (state.counter_tracker.isCounterComplete(counter)) {
			// Coroutine stack unblocked, can resume it
			coro_state.unblockCounter();
		} else {
			// Coroutine stack is still blocked awaiting something external
			return false;
		}
	}

	// XXX: when sub-tasks throw it's OK, exceptions are propagated to awaiting "parents".
	// But unhandled exceptions in the base task are silently swallowed. We should probably
	// at least warn about that and print exception details where possible. Ideally we should
	// establish some well-defined unhandled exception behavior unified with regular tasks.
	coro_state.resumeStep(coro);

	// Task is finished only when the main coroutine is done
	return coro.done();
}

// Attempts to execute task if possible (not blocked on anything).
// Returns `true` and automatically destroys task object if it was finished.
// Regular function tasks will be finished after the first call while
// coroutine tasks can require multiple entries if they suspend on something.
bool tryExecuteAndResetTask(SlaveState &state, PrivateTaskHandle &task)
{
	TaskHeader *header = task.get();

	if (header->num_wait_counters > 0) {
		// Can't run yet
		return false;
	}

	if (header->stores_coroutine) {
		if (!tryExecuteCoroutineTask(state, header->executable.coroutine.get())) {
			return false;
		}
	} else {
		// Sync point tasks can have no functor
		if (header->executable.function) [[likely]] {
			TaskContext ctx(state.task_service, task);
			// TODO: exception safety, wrap in try/catch and store the exception
			header->executable.function(ctx);
		}
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

	return true;
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

		if (!tryExecuteAndResetTask(state, state.local_waiting_queue[i])) {
			// Still not ready/finished, move this task into the first empty spot.
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

void TaskServiceSlave::threadFn(TaskService &my_service, size_t my_queue, AsyncCounterTracker &counter_tracker,
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
		if (!tryExecuteAndResetTask(state, task)) {
			// This task is not executable right away or was not done in one go (coroutine task).
			// Put it in the local queue and immediately try draining it while retaining FIFO order.
			// Previous waiting tasks might be dependencies of this one. Hence trying to execute
			// them first makes sense - might immediately unblock some waiting tasks added later.
			state.local_waiting_queue.emplace_back(std::move(task));
			tryDrainLocalQueue(state);
		} else {
			// Done!
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
