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

using RawCoroTaskHandle = std::coroutine_handle<detail::CoroTaskState>;

template<typename T>
using RawCoroSubTaskHandle = std::coroutine_handle<detail::CoroSubTaskState<T>>;

// Handle to a created task coroutine with `std::unique_ptr`-like semantics.
// To convert an arbitrary function/lambda into a task coroutine, make this type
// a return type of the function and use at least one `co_await/co_return` in its body.
//
// Then, when enqueuing this function, simply call it as argument of `TaskBuilder::enqueueTask()`.
// It will not begin executing (set to initial suspend) but will magically spawn this object.
//
// In general, you should never create nor store objects of this type anywhere in your code.
// NOTE: in any case, DO NOT call `resume()` manually, this is reserved for task service implementation.
class CoroTask final {
public:
	CoroTask(RawCoroTaskHandle handle) noexcept : m_handle(handle) {}
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

	RawCoroTaskHandle get() const noexcept { return m_handle; }

private:
	RawCoroTaskHandle m_handle;
};

namespace detail
{

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

	uint64_t blockedOnCounter() const noexcept { return m_blocked_on_counter; }

	static void *operator new(size_t bytes);
	static void *operator new(size_t bytes, std::align_val_t align);
	static void operator delete(void *ptr) noexcept;
	static void operator delete(void *ptr, std::align_val_t align) noexcept;

protected:
	uint64_t m_blocked_on_counter = 0;
	std::exception_ptr m_unhandled_exception;
};

// State block of a task coroutine. This is purely an implementation detail exposed
// because of C++ coroutine requirements. You should not create or try to access it.
// DO NOT instantiate this class manually.
//
// Class has new/delete overloads which use `PipeMemoryAllocator` so that
// all task coroutines have their state and stack frames allocated there.
class VOXEN_API CoroTaskState final : public CoroTaskStateBase {
public:
	CoroTask get_return_object() noexcept { return CoroTask(RawCoroTaskHandle::from_promise(*this)); }

	constexpr std::suspend_always initial_suspend() const noexcept { return {}; }
	constexpr void return_void() const noexcept {}

	void blockOnCounter(uint64_t counter) noexcept;
	void blockOnSubTask(CoroSubTaskStateBase *sub_task) noexcept;

	void updateSubTaskStack() noexcept;
	void resumeStep(std::coroutine_handle<> my_coro);

	void unblockCounter() noexcept { m_blocked_on_counter = 0; }

private:
	CoroSubTaskStateBase *m_sub_task_stack_top = nullptr;
};

class VOXEN_API CoroSubTaskStateBase : public CoroTaskStateBase {
public:
	constexpr std::suspend_never initial_suspend() const noexcept { return {}; }

	void blockOnCounter(uint64_t counter) noexcept;
	void blockOnSubTask(CoroSubTaskStateBase *sub_task) noexcept;

protected:
	std::coroutine_handle<> m_this_coroutine;
	CoroSubTaskStateBase *m_next_sub_task = nullptr;
	CoroSubTaskStateBase *m_prev_sub_task = nullptr;
	CoroTaskState *m_base_task = nullptr;

	friend class CoroTaskState;
};

template<typename T>
class CoroSubTaskState final : public CoroSubTaskStateBase {
public:
	CoroSubTask<T> get_return_object() noexcept
	{
		auto coro = RawCoroSubTaskHandle<T>::from_promise(*this);
		m_this_coroutine = coro;
		return CoroSubTask<T>(coro);
	}

	template<std::convertible_to<T> From>
	void return_value(From &&value) noexcept(std::is_nothrow_constructible_v<T, From>)
	{
		new (m_object_storage) T(std::forward<From>(value));
	}

	T takeObject() noexcept { return std::move(*std::launder(reinterpret_cast<T *>(m_object_storage))); }

private:
	alignas(T) std::byte m_object_storage[sizeof(T)];
};

template<>
class CoroSubTaskState<void> final : public CoroSubTaskStateBase {
public:
	CoroSubTask<void> get_return_object() noexcept;

	constexpr void return_void() const noexcept {}
};

class CoroFutureBase {
public:
	CoroFutureBase(uint64_t task_counter) noexcept : m_task_counter(task_counter) {}
	CoroFutureBase(CoroFutureBase &&) = default;
	CoroFutureBase(const CoroFutureBase &) = delete;
	CoroFutureBase &operator=(CoroFutureBase &&) = default;
	CoroFutureBase &operator=(const CoroFutureBase &) = delete;
	~CoroFutureBase() = default;

	constexpr bool await_ready() const noexcept { return false; }

	void await_suspend(RawCoroTaskHandle handle) noexcept { handle.promise().blockOnCounter(m_task_counter); }

	template<typename Y>
	void await_suspend(RawCoroSubTaskHandle<Y> handle) noexcept
	{
		handle.promise().blockOnCounter(m_task_counter);
	}

private:
	uint64_t m_task_counter;
};

} // namespace detail

template<typename T>
class CoroSubTask final {
public:
	CoroSubTask(RawCoroSubTaskHandle<T> handle) noexcept : m_handle(handle) {}
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

	void await_suspend(RawCoroTaskHandle handle) noexcept { handle.promise().blockOnSubTask(&m_handle.promise()); }

	template<typename Y>
	void await_suspend(RawCoroSubTaskHandle<Y> handle) noexcept
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
	RawCoroSubTaskHandle<T> m_handle;
};

inline CoroSubTask<void> detail::CoroSubTaskState<void>::get_return_object() noexcept
{
	auto coro = RawCoroSubTaskHandle<void>::from_promise(*this);
	m_this_coroutine = coro;
	return CoroSubTask<void>(coro);
}

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

// Specialization for `void` - this only waits for task counter completion.
// See description of the base template.
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
