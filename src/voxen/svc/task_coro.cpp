#include <voxen/svc/task_coro.hpp>

#include <voxen/common/pipe_memory_allocator.hpp>

#include <cassert>

namespace voxen::svc
{

namespace detail
{

// CoroTaskStateBase

void CoroTaskStateBase::unhandled_exception() noexcept
{
	m_unhandled_exception = std::current_exception();
}

void CoroTaskStateBase::rethrowIfHasException()
{
	if (m_unhandled_exception) {
		std::rethrow_exception(std::exchange(m_unhandled_exception, {}));
	}
}

void* CoroTaskStateBase::operator new(size_t bytes)
{
	return PipeMemoryAllocator::allocate(bytes, __STDCPP_DEFAULT_NEW_ALIGNMENT__);
}

void* CoroTaskStateBase::operator new(size_t bytes, std::align_val_t align)
{
	return PipeMemoryAllocator::allocate(bytes, static_cast<size_t>(align));
}

void CoroTaskStateBase::operator delete(void* ptr) noexcept
{
	return PipeMemoryAllocator::deallocate(ptr);
}

void CoroTaskStateBase::operator delete(void* ptr, std::align_val_t /*align*/) noexcept
{
	return PipeMemoryAllocator::deallocate(ptr);
}

// CoroTaskState

void CoroTaskState::blockOnCounter(uint64_t counter) noexcept
{
	m_blocked_on_counter = counter;
}

void CoroTaskState::blockOnSubTask(CoroSubTaskStateBase* sub_task) noexcept
{
	assert(m_blocked_on_counter == 0);
	assert(m_sub_task_stack_top == nullptr);
	// This must begin the sub-task stack
	assert(sub_task->m_prev_sub_task == nullptr);

	m_sub_task_stack_top = sub_task;
	updateSubTaskStack();
}

void CoroTaskState::updateSubTaskStack() noexcept
{
	CoroSubTaskStateBase* ptr = m_sub_task_stack_top;

	while (ptr) {
		if (ptr->m_blocked_on_counter != 0) {
			assert(m_blocked_on_counter == 0);
			// This must end the sub-task stack
			assert(ptr->m_next_sub_task == nullptr);
			// "Steal" blocked counter from the sub-task
			m_blocked_on_counter = std::exchange(ptr->m_blocked_on_counter, 0);
		}

		m_sub_task_stack_top = ptr;
		ptr->m_base_task = this;
		ptr = ptr->m_next_sub_task;
	}
}

void CoroTaskState::resumeStep(std::coroutine_handle<> my_coro)
{
	// Must not be blocked before entering this function
	assert(m_blocked_on_counter == 0);

	while (m_sub_task_stack_top) {
		CoroSubTaskStateBase* top = m_sub_task_stack_top;
		// Should be the last line in loop body but that would require one more if
		top->m_next_sub_task = nullptr;

		top->m_this_coroutine.resume();

		if (!top->m_this_coroutine.done()) {
			// Blocked on something again
			return;
		}

		// Finished, unwind the stack and continue.
		// Should not be really necessary to clear fields of `top`,
		// it's about to destroy anyway, but do it just in casee.
		top->m_base_task = nullptr;
		m_sub_task_stack_top = std::exchange(top->m_prev_sub_task, nullptr);
	}

	// All sub-task stack unwound, resume the main coroutine
	my_coro.resume();
}

// CoroSubTaskStateBase

void CoroSubTaskStateBase::blockOnCounter(uint64_t counter) noexcept
{
	// Only the sub-task stack top can block on counters
	assert(m_next_sub_task == nullptr);

	if (m_base_task) {
		m_base_task->blockOnCounter(counter);
	} else {
		m_blocked_on_counter = counter;
	}
}

void CoroSubTaskStateBase::blockOnSubTask(CoroSubTaskStateBase* sub_task) noexcept
{
	assert(m_blocked_on_counter == 0);
	assert(m_next_sub_task == nullptr);

	m_next_sub_task = sub_task;
	sub_task->m_prev_sub_task = this;

	if (m_base_task) {
		m_base_task->updateSubTaskStack();
	}
}

} // namespace detail

} // namespace voxen::svc
