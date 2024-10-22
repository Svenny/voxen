#pragma once

#include <voxen/common/pipe_memory_allocator.hpp>
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

	template<typename F>
	std::future<std::invoke_result_t<F>> enqueueTask(TaskType type, F&& f)
	{
		using R = std::invoke_result_t<F>;
		std::promise<R> promise(std::allocator_arg, TPipeMemoryAllocator<void>());
		std::future<R> future = promise.get_future();

		using PT = TPipedTask<R, F>;
		void* lpptr = PipeMemoryAllocator::allocate(sizeof(PT), alignof(PT));
		doEnqueueTask(type, new (lpptr) PT(std::move(promise), std::forward<F>(f)));

		return future;
	}

	size_t threads_count() const;

	static void initGlobalVoxenPool(size_t thread_count = 0);
	static void releaseGlobalVoxenPool();
	static ThreadPool& globalVoxenPool();

private:
	struct PipedTaskDeleter;
	struct ReportableWorkerState;
	struct ReportableWorker;

	// Extensible interface for tasks sent to thread pool.
	// Erases the underlying type (`TPipedTask` instantiation).
	// Assumes to be backed by a single `PipeMemoryAllocator` allocation.
	class IPipedTask {
	public:
		// Frees pipe memory allocation internally
		virtual ~IPipedTask() noexcept;

		// Execute the task, fulfilling its promise with value/exception.
		// Should not throw (to a slave thread) unless the failure is
		// related to the task execution system itself.
		virtual void call() = 0;
	};

	using PipedTaskPtr = std::unique_ptr<IPipedTask, PipedTaskDeleter>;

	// Actual (erased) type of `IPipedTask` implementation.
	// Behaves very much like `std::packaged_task<R()>`.
	template<typename R, typename F>
	class TPipedTask final : public IPipedTask {
	public:
		TPipedTask(std::promise<R>&& promise, F&& fn) : m_promise(std::move(promise)), m_fn(std::forward<F>(fn)) {}
		~TPipedTask() override = default;

		void call() override
		{
			try {
				if constexpr (std::is_void_v<R>) {
					m_fn();
					m_promise.set_value();
				} else {
					m_promise.set_value(m_fn());
				}
			}
			catch (...) {
				m_promise.set_exception(std::current_exception());
			}
		}

	private:
		std::promise<R> m_promise;
		F m_fn;
	};

	void doEnqueueTask(TaskType type, IPipedTask* raw_task_ptr);

	static void workerFunction(ReportableWorkerState* state);
	ReportableWorker* make_worker();
	void run_worker(ReportableWorker* worker);

private:
	std::vector<ReportableWorker*> m_workers;
	static ThreadPool* global_voxen_pool;
};

} // namespace voxen
