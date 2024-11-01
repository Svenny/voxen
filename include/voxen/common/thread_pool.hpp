#pragma once

#include <voxen/common/pipe_memory_allocator.hpp>
#include <voxen/svc/service_base.hpp>
#include <voxen/visibility.hpp>

#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>

namespace voxen
{

class VOXEN_API ThreadPool final : public svc::IService {
public:
	constexpr static UID SERVICE_UID = UID("340d78cd-5a543514-8d4a8a15-de39ab3c");

	enum class TaskType {
		// This is a CPU-bound task without particular timing restrictions
		Standard
	};

	struct Config {
		size_t thread_count = 0;
	};

	explicit ThreadPool(svc::ServiceLocator& svc, Config cfg);
	ThreadPool(ThreadPool&&) = delete;
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(ThreadPool&&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
	~ThreadPool() noexcept override;

	UID serviceUid() const noexcept override { return SERVICE_UID; }

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
	void makeWorker();

	static void workerFunction(size_t worker_index, ReportableWorkerState* state);

private:
	std::vector<std::unique_ptr<ReportableWorker>> m_workers;
};

} // namespace voxen
