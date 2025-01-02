#pragma once

#include <voxen/svc/svc_fwd.hpp>
#include <voxen/visibility.hpp>

#include <concepts>
#include <coroutine>
#include <exception>
#include <memory>
#include <new>
#include <utility>

namespace voxen::svc
{

// Handle to a created task coroutine with `std::unique_ptr`-like semantics.
// To convert an arbitrary function/lambda into a task coroutine, make this type
// a return type of the function and use at least one `co_await/co_return` in its body.
//
// Then, when enqueuing this function, simply call it as argument of `TaskBuilder::enqueueTask()`.
// It will not begin executing (set to initial suspend) but will magically spawn this object.
//
// In general, you should never need to create nor store objects of this type anywhere in your code.
// NOTE: do not attempt to manually manipulate raw coroutine handles (resume/destroy/etc.),
// this will most likely break the suspend/await logic and cause various kinds of UB.
class CoroTask final {
public:
	using RawHandle = std::coroutine_handle<detail::CoroTaskState>;

	explicit CoroTask(RawHandle handle) noexcept : m_handle(handle) {}
	CoroTask(CoroTask &&other) noexcept : m_handle(std::exchange(other.m_handle, {})) {}

	CoroTask &operator=(CoroTask &&other) noexcept
	{
		std::swap(m_handle, other.m_handle);
		return *this;
	}

	CoroTask(const CoroTask &) = delete;
	CoroTask &operator=(const CoroTask &) = delete;

	~CoroTask()
	{
		if (m_handle) {
			m_handle.destroy();
		}
	}

	// Get raw handle, intended to be used only by implementation
	RawHandle get() const noexcept { return m_handle; }

private:
	RawHandle m_handle;
};

namespace detail
{

// Base class for "promise" objects of task coroutines. This is purely
// an implementation detail exposed because of C++ coroutine requirements.
// DO NOT manuall instantiate or access this class or any of its subclasses.
//
// Class has new/delete overloads which use `PipeMemoryAllocator` so that all
// task coroutines have their state and stack frames allocated in pipe memory.
class VOXEN_API CoroTaskStateBase {
public:
	CoroTaskStateBase() = default;
	CoroTaskStateBase(CoroTaskStateBase &&) = delete;
	CoroTaskStateBase(const CoroTaskStateBase &) = delete;
	CoroTaskStateBase &operator=(CoroTaskStateBase &&) = delete;
	CoroTaskStateBase &operator=(const CoroTaskStateBase &) = delete;
	~CoroTaskStateBase() = default;

	constexpr std::suspend_always final_suspend() const noexcept { return {}; }
	void unhandled_exception() noexcept;

	void rethrowIfHasException();

	// Task counter that must complete before this coroutine can be resumed.
	// Completion is not checked, must be ensured by task service implementation.
	// 0 means the task is not blocked.
	//
	// Don't forget about "initial" waited counter set provided by `TaskBuilder`.
	// It is stored not here but inside `TaskHandle` implementation as with non-coro tasks.
	uint64_t blockedOnCounter() const noexcept { return m_blocked_on_counter; }

	static void *operator new(size_t bytes);
	static void *operator new(size_t bytes, std::align_val_t align);
	static void operator delete(void *ptr) noexcept;
	static void operator delete(void *ptr, std::align_val_t align) noexcept;

protected:
	uint64_t m_blocked_on_counter = 0;
	std::exception_ptr m_unhandled_exception;
};

// "Promise" object of `CoroTask`. Lazily-started with no return object.
// Stores the top of "await stack" of sub-tasks created inside this task.
//
class VOXEN_API CoroTaskState final : public CoroTaskStateBase {
public:
	CoroTask get_return_object() noexcept { return CoroTask(CoroTask::RawHandle::from_promise(*this)); }

	constexpr std::suspend_always initial_suspend() const noexcept { return {}; }
	constexpr void return_void() const noexcept {}

	// Mark this coroutine as blocked on task counter.
	// It must not be blocked on another counter prior to this call.
	void blockOnCounter(uint64_t counter) noexcept;
	// Mark this coroutine as blocked `co_await`-ing a sub-task.
	// It must be not blocked on anything prior to this call.
	// Sub-task stack will be automatically updated.
	//
	// `sub_task` can be itself blocked on something but it must not have any other
	// sub-task awaiting for it (as that would become an await graph instead of stack).
	void blockOnSubTask(CoroSubTaskStateBase *sub_task) noexcept;

	// Walks sub-task stack and updates its top, also "stealing" task counter
	// value if the top sub-task is blocked on it. Should be called from
	// `CoroSubTaskStateBase` when it observes a change in sub-task stack.
	void updateSubTaskStack() noexcept;

	// Resume coroutines in await stack until either the main coroutine completes
	// or some coroutine in the stack blocks again.
	// `my_coro` must be the coroutine handle related to this "promise" instance.
	// Prior to this call the coroutine stack must not be blocked on a task counter.
	void resumeStep(std::coroutine_handle<> my_coro);

	void unblockCounter() noexcept { m_blocked_on_counter = 0; }

private:
	CoroSubTaskStateBase *m_sub_task_stack_top = nullptr;
};

// Base non-templated part of `CoroSubTaskState<T>`.
// Eagerly-started with possibility to return object from `await`.
//
// Sub-tasks form "await stack" which can have either nothing or a `CoroTask`
// at the bottom. Depending on the case blocking functions behave differently.
// Unblocking can happen only in the latter case while executing in task service.
class VOXEN_API CoroSubTaskStateBase : public CoroTaskStateBase {
public:
	constexpr std::suspend_never initial_suspend() const noexcept { return {}; }

	// Mark this coroutine or its base task (if any) as blocked on task counter.
	// It must not be blocked on another counter prior to this call.
	void blockOnCounter(uint64_t counter) noexcept;
	// Mark this coroutine as blocked `co_await`-ing a sub-task.
	// It must be not blocked on anything prior to this call.
	// Sub-task stack of the base task (if any) will be automatically updated.
	//
	// `sub_task` can be itself blocked on something but it must not have any other
	// sub-task or base task awaiting for it (as that would become an await graph).
	void blockOnSubTask(CoroSubTaskStateBase *sub_task) noexcept;

protected:
	std::coroutine_handle<> m_this_coroutine;
	CoroSubTaskStateBase *m_next_sub_task = nullptr;
	CoroSubTaskStateBase *m_prev_sub_task = nullptr;
	CoroTaskState *m_base_task = nullptr;

	// Let `CoroTaskState` directly modify this object
	// instead of making many getters/setters just for it
	friend class CoroTaskState;
};

// "Promise" object of `CoroSubTask<T>`.
// Will return garbage from `co_await` if the coroutine did not call `co_return`
// before exiting (without throwing), pretty much like with regular functions.
template<typename T>
class CoroSubTaskState final : public CoroSubTaskStateBase {
public:
	CoroSubTask<T> get_return_object() noexcept
	{
		auto coro = CoroSubTask<T>::RawHandle::from_promise(*this);
		m_this_coroutine = coro;
		return CoroSubTask<T>(coro);
	}

	template<std::convertible_to<T> From>
	void return_value(From &&value) noexcept(std::is_nothrow_constructible_v<T, From>)
	{
		new (m_object_storage) T(std::forward<From>(value));
	}

	// Can be called only once in implementation of `await_resume`
	T takeObject() noexcept { return std::move(*std::launder(reinterpret_cast<T *>(m_object_storage))); }

private:
	alignas(T) std::byte m_object_storage[sizeof(T)];
};

// Specialization of `CoroSubTaskState` for `void` return type
template<>
class CoroSubTaskState<void> final : public CoroSubTaskStateBase {
public:
	CoroSubTask<void> get_return_object() noexcept;

	constexpr void return_void() const noexcept {}
};

// Base non-templated part of `CoroFuture<T>`.
// Allows to await (block) on external task counter.
class CoroFutureBase {
public:
	CoroFutureBase(uint64_t task_counter) noexcept : m_task_counter(task_counter) {}
	CoroFutureBase(CoroFutureBase &&) = default;
	CoroFutureBase(const CoroFutureBase &) = delete;
	CoroFutureBase &operator=(CoroFutureBase &&) = default;
	CoroFutureBase &operator=(const CoroFutureBase &) = delete;
	~CoroFutureBase() = default;

	constexpr bool await_ready() const noexcept { return false; }

	template<typename T>
	void await_suspend(std::coroutine_handle<T> handle) noexcept
	{
		handle.promise().blockOnCounter(m_task_counter);
	}

private:
	uint64_t m_task_counter;
};

} // namespace detail

// Handle to a created sub-task coroutine with `std::unique_ptr`-like semantics.
// To convert an arbitrary function/lambda into a sub-task coroutine, make this type
// a return type of the function and use at least one `co_await/co_return` in its body.
//
// Then, inside task coroutines you can call it like `result = co_await mySubTask(args...);`.
// It will begin executing (eagerly) and will suspend when/if sub-task blocks on an external event.
// This interface is intended for services providing "middleware" asynchronous operations like this:
//
//   class MyAsyncService {
//     CoroSubTask<OpResult> asyncOp(args...);
//   };
//
//   // A "client" task using this async service
//   CoroTask myTask(MyAsyncService &svc, ...)
//   {
//     // Prepare args...
//     // Blocks the task until `asyncOp` completes.
//     // Will either return an object or rethrow an exception.
//     MyAsyncService::OpResult res = co_await svc.asyncOp(args...);
//     // Use the result...
//   }
//
//   TaskBuilder bld(m_task_service);
//   bld.enqueueTask(myTask(m_async_service, ...));
//
// In general, you should never need to create nor store objects of this type anywhere in your code.
// NOTE: do not attempt to manually manipulate raw coroutine handles (resume/destroy/etc.)
// or call awaitable methods (`await_*()` family), this will most likely break the suspend/await
// logic and cause various kinds of UB. These methods are exposed just because of C++ requirements.
template<typename T>
class CoroSubTask final {
public:
	using RawHandle = std::coroutine_handle<detail::CoroSubTaskState<T>>;

	explicit CoroSubTask(RawHandle handle) noexcept : m_handle(handle) {}
	CoroSubTask(CoroSubTask &&other) noexcept : m_handle(std::exchange(other.m_handle, {})) {}

	CoroSubTask &operator=(CoroSubTask &&other) noexcept
	{
		std::swap(m_handle, other.m_handle);
		return *this;
	}

	CoroSubTask(const CoroSubTask &) = delete;
	CoroSubTask &operator=(const CoroSubTask &) = delete;

	~CoroSubTask()
	{
		if (m_handle) {
			m_handle.destroy();
		}
	}

	bool await_ready() const noexcept { return m_handle.done(); }

	template<typename Y>
	void await_suspend(std::coroutine_handle<Y> handle) noexcept
	{
		handle.promise().blockOnSubTask(&m_handle.promise());
	}

	T await_resume()
	{
		auto &state = m_handle.promise();
		state.rethrowIfHasException();

		if constexpr (!std::is_void_v<T>) {
			return state.takeObject();
		}
	}

private:
	RawHandle m_handle;
};

inline CoroSubTask<void> detail::CoroSubTaskState<void>::get_return_object() noexcept
{
	auto coro = CoroSubTask<void>::RawHandle::from_promise(*this);
	m_this_coroutine = coro;
	return CoroSubTask<void>(coro);
}

// Awaitable object that can be used in task (and sub-task) coroutines to model
// the behavior of `std::future<T>`, that is, waiting for an external operation
// defined by task counter and possibly returning an object.
//
// This interface is intended for services providing "middleware" asynchronous operations like this:
//
//   class AsyncIoService {
//     CoroFuture<OpResult> asyncOp(args...)
//     {
//       std::shared_ptr<OpResult> ptr = allocate...;
//       uint64_t counter = launchAsyncWork(ptr);
//       return { counter, std::move(ptr) };
//     }
//   };
//
//   // A "client" task using this async service
//   CoroTask myTask(MyAsyncService &svc, ...)
//   {
//     // Prepare args...
//     // Blocks the task until whatever was launched by `asyncOp` completes.
//     MyAsyncService::OpResult res = co_await svc.asyncOp(args...);
//     // Use the result...
//   }
//
//   TaskBuilder bld(m_task_service);
//   bld.enqueueTask(myTask(m_async_service, ...));
//
// NOTE: this is a very DIY-style and quite oversimplified primitive,
// e.g. it does not support exceptions or cancellation, and it relies
// heavily on correct usage (that task counter is valid and the return
// object will be actually written before its completion is signaled).
//
// This all means this interface is very likely to change in the future.
template<typename T = void>
class CoroFuture : public detail::CoroFutureBase {
public:
	static_assert(std::is_nothrow_move_constructible_v<T>, "Future object must be nothrow move constructible");

	CoroFuture(uint64_t task_counter, std::shared_ptr<T> object) noexcept
		: detail::CoroFutureBase(task_counter), m_object(std::move(object))
	{}

	T await_resume() noexcept { return std::move(*m_object); }

private:
	std::shared_ptr<T> m_object;
};

// Specialization of `CoroFuture` for `void`, only waits for
// task counter completion. See description of the base template.
template<>
class CoroFuture<void> : public detail::CoroFutureBase {
public:
	using detail::CoroFutureBase::CoroFutureBase;

	constexpr void await_resume() const noexcept {}
};

} // namespace voxen::svc

namespace std
{

template<typename... Args>
struct coroutine_traits<voxen::svc::CoroTask, Args...> {
	using promise_type = voxen::svc::detail::CoroTaskState;
};

template<typename T, typename... Args>
struct coroutine_traits<voxen::svc::CoroSubTask<T>, Args...> {
	using promise_type = voxen::svc::detail::CoroSubTaskState<T>;
};

} // namespace std
