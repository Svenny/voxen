#include <voxen/common/threadpool.hpp>

#include <thread>
#include <atomic>
#include <condition_variable>
#include <future>
#include <cassert>

#include <voxen/util/log.hpp>

namespace impl
{

struct ReportableWorkerState
{
	std::mutex semaphore_mutex;
	std::condition_variable semaphore;
	std::atomic_bool is_exit;

	std::mutex state_mutex;
	std::queue<std::packaged_task<void()>> tasks_queue;
};

struct ReportableWorker
{
	std::thread worker;
	ReportableWorkerState state;
};

}

namespace voxen
{

ThreadPool* ThreadPool::global_voxen_pool = nullptr;

ThreadPool::ThreadPool(int start_thread_count)
{
	if (start_thread_count == -1) {
		size_t std_hint = std::thread::hardware_concurrency();
		if (std_hint == (size_t)0)
			start_thread_count = DEFAULT_START_THREAD_COUNT;
		else
			start_thread_count = ((int)std_hint - 2); //2 already used by voxen: World thread and GUI thread
	}

	for (int i = 0; i < start_thread_count; i++)
		run_worker(make_worker());
}

ThreadPool::~ThreadPool() noexcept
{
	for (impl::ReportableWorker* worker : m_workers)
	{
		worker->state.is_exit.store(true);
		worker->state.semaphore.notify_one();
	}

	for (impl::ReportableWorker* worker : m_workers)
		if (worker->worker.joinable())
			worker->worker.join();

	while (m_workers.size() > 0)
	{
		delete m_workers.back();
		m_workers.pop_back();
	}
}

void ThreadPool::workerFunction(impl::ReportableWorkerState* state)
{
	while(!state->is_exit.load())
	{
		{
		std::unique_lock<std::mutex> lock(state->semaphore_mutex);
		state->semaphore.wait(lock);
		}

		while (task_queue_size(state) != 0)
		{
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

impl::ReportableWorker* ThreadPool::make_worker()
{
	impl::ReportableWorker* new_worker = new impl::ReportableWorker();
	new_worker->state.is_exit.store(false);
	m_workers.push_back(new_worker);
	return new_worker;
}

void ThreadPool::run_worker(impl::ReportableWorker* worker)
{
	worker->worker = std::thread(&ThreadPool::workerFunction, &worker->state);
}

void ThreadPool::cleanup_finished_workers()
{
	for (auto iter = m_workers.begin(); iter != m_workers.end(); /* no iteration here */)
	{
		if ((*iter)->state.is_exit.load())
		{
			delete *iter;
			iter = m_workers.erase(iter);
		}
		else
			iter++;
	}
}

void ThreadPool::enqueueTask(std::function<void()>&& task_function, Priority priority)
{
	//TODO(sirgienko) add support for prioritization
	(void)priority;

	std::packaged_task<void()> task(task_function);

	std::vector<int> jobs_count(m_workers.size());
	for (size_t i = 0; i < m_workers.size(); i++)
	{
		impl::ReportableWorker* worker = m_workers[i];
		std::unique_lock<std::mutex> lock(worker->state.state_mutex);
		jobs_count[i] = worker->state.tasks_queue.size();
	}
	auto iter = std::min_element(jobs_count.begin(), jobs_count.end());
	int worker_idx = iter - jobs_count.begin();
	impl::ReportableWorker* worker = m_workers[worker_idx];
	{
	std::unique_lock<std::mutex> lock(worker->state.state_mutex);
	worker->state.tasks_queue.push(std::move(task));
	worker->state.semaphore.notify_one();
	}
}

size_t ThreadPool::threads_count() const
{
	return m_workers.size();
}

size_t ThreadPool::task_queue_size(impl::ReportableWorkerState* state)
{
	std::unique_lock<std::mutex> lock(state->state_mutex);
	return state->tasks_queue.size();
}

void ThreadPool::initGlobalVoxenPool(int start_thread_count)
{
	assert(global_voxen_pool == nullptr);
	global_voxen_pool = new ThreadPool(start_thread_count);
	Log::info("Create global voxen ThreadPool with {} threads", global_voxen_pool->threads_count());
}

void ThreadPool::releaseGlobalVoxenPool()
{
	assert(global_voxen_pool);
	delete global_voxen_pool ;
	global_voxen_pool = nullptr;
}

ThreadPool& ThreadPool::globalVoxenPool()
{
	assert(global_voxen_pool);
	return *global_voxen_pool;
}

}
