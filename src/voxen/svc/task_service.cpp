#include <voxen/svc/task_service.hpp>

#include <voxen/common/pipe_memory_allocator.hpp>
#include <voxen/svc/service_locator.hpp>
#include <voxen/svc/task_handle.hpp>
#include <voxen/util/hash.hpp>
#include <voxen/util/log.hpp>

#include "task_counter_tracker.hpp"
#include "task_handle_private.hpp"
#include "task_queue_set.hpp"
#include "task_service_slave.hpp"

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
			m_slave_threads[i] = std::thread(TaskServiceSlave::threadFn, std::ref(me), i, std::ref(*m_counter_tracker),
				std::ref(m_queue_set));
		}
	}

	~TaskServiceImpl()
	{
		Log::info("Stopping task service");
		m_queue_set.requestStopAll();

		// XXX: if the system is deadlocked waiting will hang here.
		// Would be nice to detect it somehow - wait with timeout then crash?
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
