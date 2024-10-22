#include <voxen/common/thread_pool.hpp>

#include <voxen/debug/debug_uid_registry.hpp>
#include <voxen/os/futex.hpp>
#include <voxen/svc/service_locator.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/futex_work_counter.hpp>
#include <voxen/util/log.hpp>

#include <cassert>
#include <thread>

namespace voxen
{

namespace
{

// This number is subtracted from threads count returned by `std`.
// 2 loaded threads are used by Voxen: World thread and Render thread.
constexpr size_t STD_THREAD_COUNT_OFFSET = 2;
// The number of threads started in case no explicit request was made and `std`
// didn't return meaningful value. Assuming an "average" 8-threaded machine.
constexpr size_t DEFAULT_THREAD_COUNT = 8 - STD_THREAD_COUNT_OFFSET;

} // namespace

struct ThreadPool::PipedTaskDeleter {
	void operator()(IPipedTask *task) noexcept
	{
		// Deallocates itself
		task->~IPipedTask();
	}
};

ThreadPool::IPipedTask::~IPipedTask() noexcept
{
	// Deallocate here, not in `PipedTaskDeleter` - this way we will
	// not leak even if `TPipedTask` ctor fails (throws something).
	PipeMemoryAllocator::deallocate(this);
}

struct ThreadPool::ReportableWorkerState {
	FutexWorkCounter work_counter;

	os::FutexLock queue_futex;
	std::queue<PipedTaskPtr> tasks_queue;
};

struct ThreadPool::ReportableWorker {
	std::thread worker;
	ReportableWorkerState state;
};

ThreadPool::ThreadPool(svc::ServiceLocator &svc, Config cfg)
{
	svc.requestService<PipeMemoryAllocator>();

	debug::UidRegistry::registerLiteral(SERVICE_UID, "voxen/service/ThreadPool");

	if (cfg.thread_count == 0) {
		size_t std_hint = std::thread::hardware_concurrency();
		if (std_hint <= STD_THREAD_COUNT_OFFSET) {
			cfg.thread_count = DEFAULT_THREAD_COUNT;
		} else {
			cfg.thread_count = std_hint - STD_THREAD_COUNT_OFFSET;
		}
	}

	Log::info("Starting thread pool with {} threads", cfg.thread_count);
	for (size_t i = 0; i < cfg.thread_count; i++) {
		makeWorker();
	}
}

ThreadPool::~ThreadPool() noexcept
{
	for (auto &worker : m_workers) {
		worker->state.work_counter.requestStop();
	}

	for (auto &worker : m_workers) {
		if (worker->worker.joinable()) {
			worker->worker.join();
		}
	}

	m_workers.clear();
}

void ThreadPool::doEnqueueTask([[maybe_unused]] TaskType type, IPipedTask *raw_task_ptr)
{
	// Wrap it in RAII immediately
	PipedTaskPtr task(raw_task_ptr);

	// non-standard tasks are not supported yet
	assert(type == TaskType::Standard);

	size_t min_job_count = SIZE_MAX;
	ReportableWorker *min_job_thread = nullptr;

	for (auto &worker : m_workers) {
		size_t job_count = worker->state.work_counter.loadRelaxed().first;

		if (job_count < min_job_count) {
			min_job_count = job_count;
			min_job_thread = worker.get();
		}
	}
	assert(min_job_thread);

	{
		std::lock_guard lock(min_job_thread->state.queue_futex);
		min_job_thread->state.tasks_queue.emplace(std::move(task));
	}

	min_job_thread->state.work_counter.addWork(1);
}

void ThreadPool::makeWorker()
{
	auto worker = std::make_unique<ReportableWorker>();
	worker->worker = std::thread(&ThreadPool::workerFunction, &worker->state);
	m_workers.push_back(std::move(worker));
}

void ThreadPool::workerFunction(ReportableWorkerState *state)
{
	uint32_t work_remaining = 0;
	bool exit = false;

	while (!exit || work_remaining > 0) {
		std::tie(work_remaining, exit) = state->work_counter.wait();

		PipedTaskPtr task;
		uint32_t popped = 0;

		// Take the first task from the queue
		{
			std::lock_guard lock(state->queue_futex);
			if (!state->tasks_queue.empty()) {
				task = std::move(state->tasks_queue.front());
				state->tasks_queue.pop();
				popped++;
			}
		}

		while (task) {
			task->call();
			task.reset();

			// Take the next task from the queue
			std::lock_guard lock(state->queue_futex);
			if (!state->tasks_queue.empty()) {
				task = std::move(state->tasks_queue.front());
				state->tasks_queue.pop();
				popped++;
			}
		}

		std::tie(work_remaining, exit) = state->work_counter.removeWork(popped);
	}
}

} // namespace voxen
