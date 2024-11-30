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
			m_slave_threads[i] = std::thread(slaveThreadFn, std::ref(me), i, m_counter_tracker.get(), &m_queue_set);
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

	static void slaveThreadFn(TaskService &my_service, size_t my_queue, TaskCounterTracker *counter_tracker,
		TaskQueueSet *queue_set)
	{
		debug::setThreadName("ThreadPool@%zu", my_queue);

		PrivateTaskHandle task = queue_set->popTaskOrWait(my_queue);

		// When the queue returns null handle it means a stop flag was raised
		while (task.valid()) {
			TaskHeader *header = task.get();

			if (header->num_wait_counters > 0) {
				// This task is not executable right away, update its wait status
				size_t trimmed_counters = counter_tracker->trimCompleteCounters(
					std::span(header->waitCountersArray(), header->num_wait_counters));
				header->num_wait_counters = static_cast<decltype(header->num_wait_counters)>(trimmed_counters);

				// Still not ready, reschedule it to execute later.
				// TODO: this will probably interact badly with work stealing.
				// TODO: if we have nothing more to do this will degenerate to counter polling spam.
				if (trimmed_counters > 0) {
					queue_set->pushTask(my_queue, std::move(task));
					task = queue_set->popTaskOrWait(my_queue);
					continue;
				}
			}

			// Sync point tasks have no functor
			if (header->call_fn) [[likely]] {
				TaskContext ctx(my_service, task);
				// TODO: exception safety, wrap in try/catch and store the exception
				header->call_fn(header->functorStorage(), ctx);
			}

			if (!task.hasContinuations()) {
				// Signal task completion, otherwise some child will do it
				task.complete(*counter_tracker);
			}

			// Take the next task from the queue
			task = queue_set->popTaskOrWait(my_queue);
		}
	}
};

TaskService::TaskService(ServiceLocator &svc, Config cfg) : m_impl(*this, svc, patchConfig(cfg)) {}

TaskService::~TaskService() = default;

uint64_t TaskService::enqueueTask(detail::PrivateTaskHandle handle)
{
	return m_impl->enqueueTask(std::move(handle));
}

} // namespace voxen::svc
