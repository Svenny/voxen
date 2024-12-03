#include <voxen/svc/task_service.hpp>

#include <voxen/common/pipe_memory_allocator.hpp>
#include <voxen/debug/thread_name.hpp>
#include <voxen/svc/service_locator.hpp>
#include <voxen/svc/task_context.hpp>
#include <voxen/svc/task_handle.hpp>
#include <voxen/util/hash.hpp>
#include <voxen/util/log.hpp>

#include "task_counter_tracker.hpp"
#include "task_handle_private.hpp"
#include "task_queue_set.hpp"

#include <thread>

namespace voxen::svc
{

namespace
{

// This number is subtracted from threads count reported by the system.
// Expecting two major threads outside of this system: World and Render.
constexpr size_t STD_THREAD_COUNT_OFFSET = 2;
// The number of threads started in case no explicit request was made and `std`
// didn't return meaningful value. Assuming an "average" 8-threaded machine.
constexpr size_t DEFAULT_THREAD_COUNT = 8 - STD_THREAD_COUNT_OFFSET;

TaskService::Config patchConfig(TaskService::Config cfg)
{
	if (cfg.num_threads == 0) {
		size_t std_hint = std::thread::hardware_concurrency();
		if (std_hint <= STD_THREAD_COUNT_OFFSET) {
			cfg.num_threads = DEFAULT_THREAD_COUNT;
		} else {
			cfg.num_threads = std_hint - STD_THREAD_COUNT_OFFSET;
		}
	}

	return cfg;
}

} // namespace

class detail::TaskServiceImpl {
public:
	TaskServiceImpl(TaskService &me, ServiceLocator &svc, TaskService::Config cfg)
		: m_cfg(cfg)
		, m_counter_tracker(std::make_unique<TaskCounterTracker>())
		, m_queue_set(cfg.num_threads)
		, m_slave_threads(std::make_unique<std::thread[]>(cfg.num_threads))
	{
		svc.requestService<PipeMemoryAllocator>();

		Log::info("Starting task service with {} threads", cfg.num_threads);
		for (size_t i = 0; i < m_cfg.num_threads; i++) {
			m_slave_threads[i] = std::thread(slaveThreadFn, std::ref(me), i, std::ref(*m_counter_tracker), &m_queue_set);
		}
	}

	~TaskServiceImpl()
	{
		m_queue_set.requestStopAll();

		for (size_t i = 0; i < m_cfg.num_threads; i++) {
			if (m_slave_threads[i].joinable()) {
				m_slave_threads[i].join();
			}
		}
	}

	size_t eliminateCompletedWaitCounters(std::span<uint64_t> counters) noexcept
	{
		return m_counter_tracker->trimCompleteCounters(counters);
	}

	uint64_t enqueueTask(PrivateTaskHandle handle)
	{
		TaskHeader *header = handle.get();

		const uint64_t counter = m_counter_tracker->allocateCounter();
		header->task_counter = counter;

		// Select the target queue randomly.
		// XXX: might use some heuristics for more optimal scheduling,
		// e.g. prefer the current thread (if enqueueing from another task)
		// or do account for hardware topology and try threads in order of cache sharing.
		// This will get especially important if we ever launch on NUMA systems.
		uint64_t random_value = Hash::xxh64Fixed(counter ^ reinterpret_cast<uintptr_t>(header));
		size_t queue_id = random_value % m_cfg.num_threads;

		m_queue_set.pushTask(queue_id, std::move(handle));

		return counter;
	}

private:
	const TaskService::Config m_cfg;

	std::unique_ptr<TaskCounterTracker> m_counter_tracker;
	TaskQueueSet m_queue_set;
	std::unique_ptr<std::thread[]> m_slave_threads;

	static void slaveThreadFn(TaskService &my_service, size_t my_queue, TaskCounterTracker &counter_tracker,
		TaskQueueSet *queue_set)
	{
		debug::setThreadName("ThreadPool@%zu", my_queue);

		// Store waiting tasks locally to not keep them in the limited ring buffer.
		// Unless we can somehow reschedule them in a cache-aware way, there is
		// not much sense moving them to any other thread.
		std::vector<PrivateTaskHandle> local_waiting_queue;

		auto execute_task = [&](PrivateTaskHandle &task) {
			TaskHeader *header = task.get();

			// Sync point tasks have no functor
			if (header->call_fn) [[likely]] {
				TaskContext ctx(my_service, task);
				// TODO: exception safety, wrap in try/catch and store the exception
				header->call_fn(header->functorStorage(), ctx);
			}

			if (!task.hasContinuations()) {
				// Signal task completion, otherwise some child will do it
				task.complete(counter_tracker);
			}
		};

		// Update wait status of all tasks in the local queue and execute them if possible.
		// Removes executed task handles, does not change the order of the rest.
		auto try_drain_local_queue = [&] {
			size_t remaining_tasks = 0;

			for (size_t i = 0; i < local_waiting_queue.size(); i++) {
				TaskHeader *header = local_waiting_queue[i].get();

				size_t remaining_counters = counter_tracker.trimCompleteCounters(
					std::span(header->waitCountersArray(), header->num_wait_counters));
				header->num_wait_counters = static_cast<decltype(header->num_wait_counters)>(remaining_counters);

				if (remaining_counters == 0) {
					// Ready now, execute and reset it, leaving an empty spot in the vector
					execute_task(local_waiting_queue[i]);
					local_waiting_queue[i].reset();
				} else {
					// Still not ready, move this task into the first empty spot.
					// If no task was reset in the above branch yet, this will just swap with itself.
					std::swap(local_waiting_queue[i], local_waiting_queue[remaining_tasks]);
					remaining_tasks++;
				}
			}

			// Remove null handles from executed tasks
			local_waiting_queue.erase(local_waiting_queue.begin() + ptrdiff_t(remaining_tasks), local_waiting_queue.end());
		};

		PrivateTaskHandle task = queue_set->popTaskOrWait(my_queue);
		size_t executed_independent_tasks = 0;

		// When the queue returns null handle it means a stop flag was raised
		while (task.valid()) {
			TaskHeader *header = task.get();

			if (header->num_wait_counters > 0) {
				// This task might be not executable right away.
				// Put it in the local queue and immediately try draining it while retaining FIFO order.
				// Previous waiting tasks might be dependencies of this one. Hence trying to execute
				// them first makes sense - might immediately unblock some waiting tasks added later.
				local_waiting_queue.emplace_back(std::move(task));
				try_drain_local_queue();
			} else {
				// Task without dependencies, execute it right away
				execute_task(task);

				executed_independent_tasks++;
				if (!local_waiting_queue.empty() && executed_independent_tasks > 50) {
					// Avoid large runs of independent tasks without checking local queue
					try_drain_local_queue();
					executed_independent_tasks = 0;
				}
			}

			// Take the next task from the queue.
			// We can't call `popTaskOrWait` while we have any waiting tasks. It will
			// deadlock the system if these waiting tasks are themselves being waited on.
			if (!local_waiting_queue.empty()) {
				// Try taking the task without waiting
				task = queue_set->tryPopTask(my_queue);

				// If we've received a valid handle, then just continue the main loop
				// trying to execute it. Otherwise we know the input queue is empty and
				// we have nothing to do for a while - might go over waiting tasks
				// in the meantime and then try getting a handle again. Unless
				// the system is deadlocked, we are guaranteed to eventually drain
				// the waiting queue (in finite time) and exit this loop.
				while (!task.valid() && !local_waiting_queue.empty()) {
					try_drain_local_queue();
					task = queue_set->tryPopTask(my_queue);
				}

				// If the above loop stopped because the waiting queue got empty
				// but the task handle is still null, wait for it or the main loop
				// condition will confuse it with stop flag and exit the thread.
				if (!task.valid()) {
					task = queue_set->popTaskOrWait(my_queue);
				}
			} else {
				// Wait (sleep) until the next task comes in.
				// If this returns null handle then a stop flag was raised.
				task = queue_set->popTaskOrWait(my_queue);
			}
		}
	}
};

TaskService::TaskService(ServiceLocator &svc, Config cfg) : m_impl(*this, svc, patchConfig(cfg)) {}

TaskService::~TaskService() = default;

size_t TaskService::eliminateCompletedWaitCounters(std::span<uint64_t> counters) noexcept
{
	return m_impl->eliminateCompletedWaitCounters(counters);
}

uint64_t TaskService::enqueueTask(detail::PrivateTaskHandle handle)
{
	return m_impl->enqueueTask(std::move(handle));
}

} // namespace voxen::svc
