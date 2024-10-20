#pragma once

#include <voxen/visibility.hpp>

// Work around clang bug (emits warning for some deprecated shit in std headers)
// Like this: https://github.com/llvm/llvm-project/issues/76515
//
// Wrapping just problematic `std::bind` usage is not enough,
// warning triggers at `enqueueTask()` call sites.
// I have no idea how this works... note I'm setting it to error, basically should change
// nothing as we're already building with -Werror. But somehow it does suppress warning
// from std headers while retaining errors in user code (try adding [[deprecated]] somewhere).
#pragma clang diagnostic error "-Wdeprecated-declarations"

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

#ifndef _WIN32
		std::packaged_task<R()> task { std::bind(std::forward<F>(f), std::forward<Args>(args)...) };
		std::future<R> future = task.get_future();

		doEnqueueTask(type, std::packaged_task<void()> { std::move(task) });
#else
		// MS STL bug that remains unfixed for years:
		// https://developercommunity.visualstudio.com/t/unable-to-move-stdpackaged-task-into-any-stl-conta/108672
		// Ugly workaround for now, we'll replace this whole part with custom function storage later.
		std::promise<R> prom;
		std::future<R> future = prom.get_future();

		auto task = [tprom = std::move(prom), tf = std::forward<F>(f), ... targs = std::forward<Args>(args)]() mutable {
			try {
				if constexpr (std::is_void_v<R>) {
					std::invoke(std::forward<F>(tf), std::forward<Args>(targs)...);
					tprom.set_value();
				} else {
					tprom.set_value(std::invoke(std::forward<F>(tf), std::forward<Args>(targs)...));
				}
			}
			catch (...) {
				tprom.set_exception(std::current_exception());
			}
		};
		auto taskptr = std::make_shared<decltype(task)>(std::move(task));

		doEnqueueTask(type, std::packaged_task<void()>([taskptr] { taskptr->operator()(); }));
#endif

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
	ReportableWorker* make_worker();
	void run_worker(ReportableWorker* worker);

private:
	std::vector<ReportableWorker*> m_workers;
	static ThreadPool* global_voxen_pool;
};

} // namespace voxen
