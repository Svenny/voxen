#include <voxen/svc/task_coro.hpp>

#include <voxen/common/pipe_memory_allocator.hpp>

namespace voxen::svc
{

// CoroTaskHandle

void CoroTaskHandle::resume() const
{
	CoroTaskState& state = m_handle.promise();
	RawCoroSubTaskHandle sub_task = state.callStackTop();

	while (sub_task) {
		// Sub-tasks have "never" final suspend, meaning they destroy themselves
		// after completing. So `sub_task` can become dangling immediately after
		// this line.
		// Also, dtor of `CoroSubTaskState` replaces stack top automatically.
		// As we can't use `done()` anymore, check if the stack top has changed
		// to determine whether this sub-task has completed or is suspended again.
		sub_task.resume();

		RawCoroSubTaskHandle new_stack_top = state.callStackTop();
		if (new_stack_top == sub_task) {
			// Suspended again in this stack frame, return
			return;
		}

		// This stack frame has finished, proceed to the next one
		sub_task = new_stack_top;
	}

	// No call stack (or everything unwound), resume the base task
	m_handle.resume();
}

// CoroTaskState

void CoroTaskState::unhandled_exception() noexcept
{
	m_exception = std::current_exception();
}

void CoroTaskState::retrowIfHasException()
{
	if (m_exception) {
		std::rethrow_exception(std::exchange(m_exception, {}));
	}
}

void* CoroTaskState::operator new(size_t bytes)
{
	return PipeMemoryAllocator::allocate(bytes, __STDCPP_DEFAULT_NEW_ALIGNMENT__);
}

void* CoroTaskState::operator new(size_t bytes, std::align_val_t align)
{
	return PipeMemoryAllocator::allocate(bytes, static_cast<size_t>(align));
}

void CoroTaskState::operator delete(void* ptr) noexcept
{
	return PipeMemoryAllocator::deallocate(ptr);
}

void CoroTaskState::operator delete(void* ptr, std::align_val_t /*align*/) noexcept
{
	return PipeMemoryAllocator::deallocate(ptr);
}

// CoroSubTaskState

CoroSubTaskState::CoroSubTaskState(RawCoroTaskHandle base_task) noexcept : m_base_task(base_task)
{
	// We're the top of call stack now
	m_previous_task = m_base_task.promise().callStackTop();
	m_base_task.promise().setCallStackTop(RawCoroSubTaskHandle::from_promise(*this));
}

CoroSubTaskState::~CoroSubTaskState()
{
	// Pop this frame from call stack
	m_base_task.promise().setCallStackTop(m_previous_task);
}

void CoroSubTaskState::unhandled_exception() noexcept
{
	// Store exception in the base task. This stack frame is done at this point,
	// exception will be retrown upon resuming the previous frame.
	m_base_task.promise().unhandled_exception();
}

void* CoroSubTaskState::operator new(size_t bytes)
{
	return PipeMemoryAllocator::allocate(bytes, __STDCPP_DEFAULT_NEW_ALIGNMENT__);
}

void* CoroSubTaskState::operator new(size_t bytes, std::align_val_t align)
{
	return PipeMemoryAllocator::allocate(bytes, static_cast<size_t>(align));
}

void CoroSubTaskState::operator delete(void* ptr) noexcept
{
	return PipeMemoryAllocator::deallocate(ptr);
}

void CoroSubTaskState::operator delete(void* ptr, std::align_val_t /*align*/) noexcept
{
	return PipeMemoryAllocator::deallocate(ptr);
}

// CoroTaskAwaitableBaseDetail

void CoroTaskAwaitableBaseDetail::blockOnCounter(uint64_t counter) noexcept
{
	m_base_task->setBlockedOnCounter(counter);
}

void CoroTaskAwaitableBaseDetail::onSuspend(RawCoroTaskHandle handle) noexcept
{
	m_base_task = &handle.promise();
}

void CoroTaskAwaitableBaseDetail::onResume() const
{
	// Exception pointer can become non-zero only if there was an unhandled exception
	// somewhere in the call stack. Assuming this method is called on *every* resume
	// we'll correctly propagate exceptions down the stack, basically unwinding it.
	m_base_task->retrowIfHasException();
}
RawCoroTaskHandle CoroTaskAwaitableBaseDetail::getBaseTaskHandle(RawCoroSubTaskHandle handle) noexcept
{
	return handle.promise().baseTaskHandle();
}

} // namespace voxen::svc
