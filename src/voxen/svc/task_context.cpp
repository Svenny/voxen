#include <voxen/svc/task_context.hpp>

#include "task_handle_private.hpp"

namespace voxen::svc
{

uint64_t TaskContext::getThisTaskCounter() noexcept
{
	return m_handle.getCounter();
}

detail::TaskHeader *TaskContext::getThisTaskHeader() noexcept
{
	return m_handle.get();
}

} // namespace voxen::svc
