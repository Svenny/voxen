#include <voxen/common/threadpool.hpp>

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <future>
#include <thread>

#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

namespace voxen
{

// This number is subtracted from threads count returned by `std`.
// 2 loaded threads are used by Voxen: World thread and GUI thread.
constexpr static size_t STD_THREAD_COUNT_OFFSET = 2;
// The number of threads started in case no explicit request was made and `std`
// didn't return meaningful value. Assuming an "average" 8-threaded machine.
constexpr static size_t DEFAULT_THREAD_COUNT = 8 - STD_THREAD_COUNT_OFFSET;

struct ThreadPool::ReportableWorkerState {
	std::mutex semaphore_mutex;
	std::condition_variable semaphore;
	std::atomic_bool is_exit;

	std::mutex state_mutex;
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
		std::unique_lock lock(worker->state.semaphore_mutex);

		worker->state.is_exit.store(true);
		worker->state.semaphore.notify_one();
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
		size_t job_count;

		{
			std::unique_lock<std::mutex> lock(worker->state.state_mutex);
			job_count = worker->state.tasks_queue.size();
		}

		if (job_count < min_job_count) {
			min_job_count = job_count;
			min_job_thread = worker;
		}
	}
	assert(min_job_thread);

	std::unique_lock<std::mutex> lock(min_job_thread->state.state_mutex);
	min_job_thread->state.tasks_queue.emplace(std::move(task));
	min_job_thread->state.semaphore.notify_one();
}

void ThreadPool::workerFunction(ReportableWorkerState* state)
{
	std::unique_lock semaphore_lock(state->semaphore_mutex);

	while (!state->is_exit.load()) {
		state->semaphore.wait(semaphore_lock);

		while (task_queue_size(state) != 0) {
			std::packaged_task<void()> task;
			{
				std::unique_lock<std::mutex> lock(state->state_mutex);
				task = std::move(state->tasks_queue.front());
				state->tasks_queue.pop();
			}

			task();
		}
		state->state_mutex.unlock();
	}
}

ThreadPool::ReportableWorker* ThreadPool::make_worker()
{
	auto new_worker = new ReportableWorker();
	new_worker->state.is_exit.store(false);
	m_workers.push_back(new_worker);
	return new_worker;
}

void ThreadPool::run_worker(ReportableWorker* worker)
{
	worker->worker = std::thread(&ThreadPool::workerFunction, &worker->state);
}

void ThreadPool::cleanup_finished_workers()
{
	for (auto iter = m_workers.begin(); iter != m_workers.end(); /* no iteration here */) {
		if ((*iter)->state.is_exit.load()) {
			delete *iter;
			iter = m_workers.erase(iter);
		} else {
			iter++;
		}
	}
}

size_t ThreadPool::threads_count() const
{
	return m_workers.size();
}

size_t ThreadPool::task_queue_size(ReportableWorkerState* state)
{
	std::unique_lock<std::mutex> lock(state->state_mutex);
	return state->tasks_queue.size();
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
