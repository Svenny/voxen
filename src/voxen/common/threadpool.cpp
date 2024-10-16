#include <voxen/common/threadpool.hpp>

#include <voxen/os/futex.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/futex_work_counter.hpp>
#include <voxen/util/log.hpp>

#include <cassert>
#include <future>
#include <thread>

namespace voxen
{

// This number is subtracted from threads count returned by `std`.
// 2 loaded threads are used by Voxen: World thread and GUI thread.
constexpr static size_t STD_THREAD_COUNT_OFFSET = 2;
// The number of threads started in case no explicit request was made and `std`
// didn't return meaningful value. Assuming an "average" 8-threaded machine.
constexpr static size_t DEFAULT_THREAD_COUNT = 8 - STD_THREAD_COUNT_OFFSET;

struct ThreadPool::ReportableWorkerState {
	FutexWorkCounter work_counter;

	os::FutexLock queue_futex;
	std::queue<std::packaged_task<void()>> tasks_queue;
};

struct ThreadPool::ReportableWorker {
	std::thread worker;
	ReportableWorkerState state;
};

ThreadPool* ThreadPool::global_voxen_pool = nullptr;

ThreadPool::ThreadPool(size_t thread_count)
{
	if (thread_count == 0) {
		size_t std_hint = std::thread::hardware_concurrency();
		if (std_hint <= STD_THREAD_COUNT_OFFSET) {
			thread_count = DEFAULT_THREAD_COUNT;
		} else {
			thread_count = std_hint - STD_THREAD_COUNT_OFFSET;
		}
	}

	Log::info("Starting thread pool with {} threads", thread_count);
	for (size_t i = 0; i < thread_count; i++) {
		run_worker(make_worker());
	}
}

ThreadPool::~ThreadPool() noexcept
{
	for (ReportableWorker* worker : m_workers) {
		worker->state.work_counter.requestStop();
	}

	for (ReportableWorker* worker : m_workers) {
		if (worker->worker.joinable()) {
			worker->worker.join();
		}
	}

	while (!m_workers.empty()) {
		delete m_workers.back();
		m_workers.pop_back();
	}
}

void ThreadPool::doEnqueueTask([[maybe_unused]] TaskType type, std::packaged_task<void()> task)
{
	// non-standard tasks are not supported yet
	assert(type == TaskType::Standard);

	size_t min_job_count = SIZE_MAX;
	ReportableWorker* min_job_thread = nullptr;

	for (ReportableWorker* worker : m_workers) {
		size_t job_count = worker->state.work_counter.loadRelaxed().first;

		if (job_count < min_job_count) {
			min_job_count = job_count;
			min_job_thread = worker;
		}
	}
	assert(min_job_thread);

	{
		std::lock_guard lock(min_job_thread->state.queue_futex);
		min_job_thread->state.tasks_queue.emplace(std::move(task));
	}

	min_job_thread->state.work_counter.addWork(1);
}

void ThreadPool::workerFunction(ReportableWorkerState* state)
{
	uint32_t work_remaining = 0;
	bool exit = false;

	while (!exit || work_remaining > 0) {
		std::tie(work_remaining, exit) = state->work_counter.wait();

		std::packaged_task<void()> task;
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

		while (task.valid()) {
			task();

			// Take the next task from the queue
			{
				std::lock_guard lock(state->queue_futex);
				if (state->tasks_queue.empty()) {
					task = {};
				} else {
					task = std::move(state->tasks_queue.front());
					state->tasks_queue.pop();
					popped++;
				}
			}
		}

		std::tie(work_remaining, exit) = state->work_counter.removeWork(popped);
	}
}

ThreadPool::ReportableWorker* ThreadPool::make_worker()
{
	auto new_worker = new ReportableWorker();
	m_workers.push_back(new_worker);
	return new_worker;
}

void ThreadPool::run_worker(ReportableWorker* worker)
{
	worker->worker = std::thread(&ThreadPool::workerFunction, &worker->state);
}

size_t ThreadPool::threads_count() const
{
	return m_workers.size();
}

void ThreadPool::initGlobalVoxenPool(size_t thread_count)
{
	assert(global_voxen_pool == nullptr);
	global_voxen_pool = new ThreadPool(thread_count);
	Log::info("Create global voxen ThreadPool with {} threads", global_voxen_pool->threads_count());
}

void ThreadPool::releaseGlobalVoxenPool()
{
	assert(global_voxen_pool);
	delete global_voxen_pool;
	global_voxen_pool = nullptr;
}

ThreadPool& ThreadPool::globalVoxenPool()
{
	assert(global_voxen_pool);
	return *global_voxen_pool;
}

} // namespace voxen
