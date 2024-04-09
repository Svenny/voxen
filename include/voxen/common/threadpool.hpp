#pragma once

#include <voxen/visibility.hpp>

#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>

namespace voxen
{

template<typename T>
class ThreadPoolResultsQueue {
public:
	ThreadPoolResultsQueue(const ThreadPoolResultsQueue& other) = delete;
	ThreadPoolResultsQueue(ThreadPoolResultsQueue&& other) = default;
	~ThreadPoolResultsQueue() noexcept {}

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

class VOXEN_API ThreadPool {
public:
	enum class TaskType {
		// This is a CPU-bound task without particular timing restrictions
		Standard
	};

	explicit ThreadPool(size_t thread_count = 0);
	ThreadPool(ThreadPool&&) = delete;
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(ThreadPool&&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
	~ThreadPool() noexcept;

	template<typename F, typename... Args>
	std::future<std::invoke_result_t<F, Args...>> enqueueTask(TaskType type, F&& f, Args&&... args)
	{
		using R = std::invoke_result_t<F, Args...>;

		std::promise<R> promise;
		std::future<R> future = promise.get_future();

		doEnqueueTask(type,
			std::packaged_task<void()> {
				[promise = std::move(promise),
					task = std::bind(std::forward<F>(f), std::forward<Args>(args)...)]() mutable {
					try {
						if constexpr (std::is_same_v<R, void>) {
							task();
							promise.set_value();
						} else {
							promise.set_value(task());
						}
					}
					catch (...) {
						promise.set_exception(std::current_exception());
					}
				} });

		return future;
	}

	size_t threads_count() const;

	static void initGlobalVoxenPool(size_t thread_count = 0);
	static void releaseGlobalVoxenPool();
	static ThreadPool& globalVoxenPool();

private:
	struct ReportableWorkerState;
	struct ReportableWorker;

	void doEnqueueTask(TaskType type, std::packaged_task<void()> task);

	static void workerFunction(ReportableWorkerState* state);
	static size_t task_queue_size(ReportableWorkerState* state);
	ReportableWorker* make_worker();
	void run_worker(ReportableWorker* worker);
	void cleanup_finished_workers();

private:
	std::vector<ReportableWorker*> m_workers;
	static ThreadPool* global_voxen_pool;
};

} // namespace voxen
