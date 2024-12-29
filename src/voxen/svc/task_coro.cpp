#include <voxen/svc/task_coro.hpp>

#include <voxen/common/pipe_memory_allocator.hpp>

namespace voxen::svc
{

void CoroTaskState::unhandled_exception() noexcept
{
	m_exception = std::current_exception();
	// `final_suspend` is called after this so the coroutine will be considered done
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

} // namespace voxen::svc
