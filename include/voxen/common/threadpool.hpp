#pragma once

#include <mutex>
#include <queue>
#include <memory>
#include <functional>

namespace impl
{
	struct ReportableWorkerState;
	struct ReportableWorker;
}

namespace voxen
{

template <typename T>
class ThreadPoolResultsQueue {
public:
	ThreadPoolResultsQueue(const ThreadPoolResultsQueue& other) = delete;
	ThreadPoolResultsQueue(ThreadPoolResultsQueue&& other) = default;
	~ThreadPoolResultsQueue() noexcept
	{
	}

	static std::shared_ptr<ThreadPoolResultsQueue<T>> createPoolQueue()
	{
		return std::shared_ptr<ThreadPoolResultsQueue<T>>(new ThreadPoolResultsQueue());
	}

	bool isEmpty()
	{
		std::unique_lock<std::mutex> lock(m_mutex);

		return m_data.size() == 0;
	}

	void push(T&& obj)
	{
		std::unique_lock<std::mutex> lock(m_mutex);

		m_data.push(obj);
	}

	T pop()
	{
		std::unique_lock<std::mutex> lock(m_mutex);

		T value = m_data.front();
		m_data.pop();
		return value;
	}

private:
	ThreadPoolResultsQueue() = default;

private:
	std::queue<T> m_data;
	std::mutex m_mutex;
};

class ThreadPool {
public:
	enum class Priority {
		Urgent, High, Medium, Low
	};

public:
	ThreadPool(int start_thread_count = -1);
	ThreadPool(const ThreadPool& other) = delete;
	ThreadPool(ThreadPool&& other) = delete;
	~ThreadPool() noexcept;

	void enqueueTask(std::function<void()>&& task_function, Priority priority = Priority::Medium);

	size_t threads_count() const;

	static void initGlobalVoxenPool(int start_thread_count = -1);
	static void releaseGlobalVoxenPool();
	static ThreadPool& globalVoxenPool();

private:
	static void workerFunction(impl::ReportableWorkerState* state);
	static size_t task_queue_size(impl::ReportableWorkerState* state);
	impl::ReportableWorker* make_worker();
	void run_worker(impl::ReportableWorker* worker);
	void cleanup_finished_workers();

private:
	std::vector<impl::ReportableWorker*> m_workers;
	static ThreadPool* global_voxen_pool;

	constexpr static int DEFAULT_START_THREAD_COUNT = 6;
};

}
