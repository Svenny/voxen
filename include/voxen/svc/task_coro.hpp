#pragma once

#include <voxen/svc/svc_fwd.hpp>
#include <voxen/visibility.hpp>

#include <coroutine>
#include <exception>
#include <new>
#include <utility>

namespace voxen::svc
{

// Handle to a created task coroutine with `std::unique_ptr`-like semantics.
// To convert an arbitrary function/labmda into a task coroutine, make this type
// a return type of the function and use at least one `co_await/co_return` in its body.
//
// Then, when enqueuing this function, simply call it as argument of `TaskBuilder::enqueueTask()`.
// It will not begin executing (set to initial suspend) but will magically spawn this object.
//
// In general, you should never create nor store objects of this type anywhere in your code.
// NOTE: in any case, DO NOT call `resume()` manually, this is reserved for task service implementation.
class VOXEN_API CoroTaskHandle {
public:
	CoroTaskHandle(std::coroutine_handle<CoroTaskState> handle) noexcept : m_handle(handle) {}
	CoroTaskHandle(CoroTaskHandle &&other) noexcept : m_handle(std::exchange(other.m_handle, {})) {}

	CoroTaskHandle &operator=(CoroTaskHandle &&other) noexcept
	{
		std::swap(m_handle, other.m_handle);
		return *this;
	}

	CoroTaskHandle(const CoroTaskHandle &) = delete;
	CoroTaskHandle &operator=(const CoroTaskHandle &) = delete;

	~CoroTaskHandle()
	{
		if (m_handle) {
			m_handle.destroy();
		}
	}

	// Whether the coroutine has finished executing (UB for empty handles)
	bool done() const noexcept { return m_handle.done(); }
	// Whether the coroutine is valid to execute
	bool runnable() const noexcept { return m_handle && !m_handle.done(); }
	// Execute the coroutine until the next suspension point (UB for empty/done handles).
	// DO NOT call outside of task service implementation!
	void resume() const { m_handle.resume(); }

	// Access the coroutine state block (UB for empty handles)
	CoroTaskState &state() const noexcept { return m_handle.promise(); }

private:
	std::coroutine_handle<CoroTaskState> m_handle;
};

// State block of a task coroutine. This is purely an implementation detail exposed
// because of C++ coroutine requirements. You should not create or try to access it.
// DO NOT instantiate this class manually.
//
// Class has new/delete overloads which use `PipeMemoryAllocator` so that
// all task coroutines have their state and stack frames allocated there.
class VOXEN_API CoroTaskState {
public:
	CoroTaskState() = default;
	CoroTaskState(CoroTaskState &&) = delete;
	CoroTaskState(const CoroTaskState &) = delete;
	CoroTaskState &operator=(CoroTaskState &&) = delete;
	CoroTaskState &operator=(const CoroTaskState &) = delete;
	~CoroTaskState() = default;

	CoroTaskHandle get_return_object() noexcept
	{
		return CoroTaskHandle(std::coroutine_handle<CoroTaskState>::from_promise(*this));
	}

	constexpr std::suspend_always initial_suspend() const noexcept { return {}; }
	constexpr std::suspend_always final_suspend() noexcept { return {}; }
	constexpr void return_void() const noexcept {}

	CoroTaskContext *context() const noexcept { return m_context; }
	void setContext(CoroTaskContext *context) noexcept { m_context = context; }

	uint64_t blockedOnCounter() const noexcept { return m_blocked_on_counter; }
	void setBlockedOnCounter(uint64_t counter) noexcept { m_blocked_on_counter = counter; }

	void unhandled_exception() noexcept;

	static void *operator new(size_t bytes);
	static void *operator new(size_t bytes, std::align_val_t align);
	static void operator delete(void *ptr) noexcept;
	static void operator delete(void *ptr, std::align_val_t align) noexcept;

private:
	CoroTaskContext *m_context = nullptr;
	uint64_t m_blocked_on_counter = 0;
	std::exception_ptr m_exception;
};

// Helper class analogous to `TaskContext` for regular function (non-coroutine) tasks.
// Not passed as argument but accessed from inside the coroutine with an awkward
// `co_await CoroTaskContext::current()` - task service will magically fill it.
class VOXEN_API CoroTaskContext {
private:
	struct GetAwaitable {
		constexpr bool await_ready() const noexcept { return false; }
		bool await_suspend(std::coroutine_handle<CoroTaskState> handle) noexcept
		{
			ctx = handle.promise().context();
			return false;
		}
		CoroTaskContext &await_resume() const noexcept { return *ctx; }

		CoroTaskContext *ctx = nullptr;
	};

	struct CounterAwaitable {
		constexpr bool await_ready() const noexcept { return false; }
		void await_suspend(std::coroutine_handle<CoroTaskState> handle) noexcept
		{
			handle.promise().setBlockedOnCounter(counter);
		}
		constexpr void await_resume() const noexcept {}

		uint64_t counter = 0;
	};

public:
	explicit CoroTaskContext(TaskService &svc) noexcept : m_task_service(svc) {}
	CoroTaskContext(CoroTaskContext &&) = delete;
	CoroTaskContext(const CoroTaskContext &) = delete;
	CoroTaskContext &operator=(CoroTaskContext &&) = delete;
	CoroTaskContext &operator=(const CoroTaskContext &) = delete;
	~CoroTaskContext() = default;

	// C++ coroutine semantics are a bit awkward and don't allow getting things
	// related to coroutine state directly so use this construction:
	//     CoroTaskContext &ctx = co_await CoroTaskContext::current();
	//
	// NOTE: this will UB wildly if you call `CoroTaskHandle::resume()` manually,
	// for the task state block is not fully initialized before it is enqueued.
	constexpr static GetAwaitable current() noexcept { return {}; }

	// Task service executing this. You can create `TaskBuilder`
	// from it to launch independent, non-continuation tasks.
	TaskService &taskService() noexcept { return m_task_service; }

	// Block this corouting on task counter. Use to create task dependencies
	// dynamically during execution, exactly what coroutines are needed for:
	//     uint64_t counter = startSomeAsyncOperation();
	//     co_await ctx.waitTaskCounter(counter);
	CounterAwaitable waitTaskCounter(uint64_t counter) noexcept { return CounterAwaitable { counter }; }

private:
	TaskService &m_task_service;
};

} // namespace voxen::svc

namespace std
{

template<typename... Args>
struct coroutine_traits<voxen::svc::CoroTaskHandle, Args...> {
	using promise_type = voxen::svc::CoroTaskState;
};

} // namespace std
