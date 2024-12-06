#include <voxen/svc/task_builder.hpp>

#include <voxen/common/pipe_memory_allocator.hpp>
#include <voxen/debug/bug_found.hpp>
#include <voxen/svc/task_context.hpp>
#include <voxen/svc/task_service.hpp>

#include "task_handle_private.hpp"

#include <vector>

namespace voxen::svc
{

static_assert(alignof(detail::TaskHeader) >= alignof(uint64_t), "TaskHeader has insufficient alignment");

struct TaskBuilder::Impl {
	TaskService &service;

	std::vector<uint64_t> wait_counters;
	uint64_t last_task_counter = 0;
	detail::PrivateTaskHandle last_task_handle;
	// We don't take ref - it can be non-null only within functor scope where it can't destroy anyway
	detail::TaskHeader *parent_task_header = nullptr;
};

TaskBuilder::TaskBuilder(TaskService &svc) : m_impl(svc) {}

TaskBuilder::TaskBuilder(TaskContext &ctx) : m_impl(ctx.taskService())
{
	m_impl->parent_task_header = ctx.getThisTaskHeader();
}

TaskBuilder::~TaskBuilder() = default;

void TaskBuilder::addWait(uint64_t counter)
{
	m_impl->wait_counters.emplace_back(counter);
}

void TaskBuilder::addWait(std::span<const uint64_t> counters)
{
	auto &wait_cnt = m_impl->wait_counters;
	wait_cnt.insert(wait_cnt.end(), counters.begin(), counters.end());
}

TaskHandle TaskBuilder::enqueueSyncPoint()
{
	// Quite literally an empty task enqueue
	createTaskHandle(0);
	return doEnqueueTaskWithHandle();
}

uint64_t TaskBuilder::getLastTaskCounter() const noexcept
{
	return m_impl->last_task_counter;
}

void *TaskBuilder::createTaskHandle(size_t functor_size)
{
	auto &wait_cnt = m_impl->wait_counters;

	// Eliminate zero counters - these are trivially complete.
	//
	// XXX: it's questionable whether checking for other completed counters here makes sense.
	// On one hand, this can trim the set and maybe even make the task immediately executable.
	// On the other hand, checking counters should happen as close to execution attempt
	// as possible - there will be more chances to have completed counters then.
	//
	// Probably a "fast trim" function in `TaskCounterTracker` would do it.
	// It could quickly check against "fully completed" counters without any locking.
	// Maybe even periodically copy those values into a dense (not cache-line aligned)
	// array where no atomics are performed, so it's mostly read-only data and we'll get
	// no unwanted cache coherency traffic by accessing it here.
	auto first_zero = std::remove(wait_cnt.begin(), wait_cnt.end(), 0);
	wait_cnt.erase(first_zero, wait_cnt.end());

	// We have static asserts that there is enough alignment for counters and functor
	size_t size = sizeof(detail::TaskHeader);
	size += sizeof(uint64_t) * wait_cnt.size();

	// `num_wait_counters` and `functor_storage_offset` are 16 bits.
	// Counters are before the functor and are 8 bytes each, their total size will
	// overflow uint16 (in `functor_storage_offset`) much faster than their number.
	// So enough to do one check.
	// This is achievable in practice, make sure the bug never goes unnoticed.
	//
	// TODO: trim/wait if there are more counters (might happen on large system shutdown)
	if (size > UINT16_MAX) [[unlikely]] {
		// Not that the user can somehow recover from this error
		debug::bugFound("TaskBuilder: uint16 overflow in task header; too many wait counters");
	}

	// Don't forget to add functor size after the check
	size += functor_size;

	void *place = PipeMemoryAllocator::allocate(size, alignof(detail::TaskHeader));
	auto *header = new (place) detail::TaskHeader();

	header->num_wait_counters = static_cast<uint16_t>(wait_cnt.size());
	header->functor_storage_offset = static_cast<uint16_t>(size - functor_size);
	header->parent_handle.setParent(m_impl->parent_task_header);
	// Write wait counters array
	std::copy_n(wait_cnt.data(), wait_cnt.size(), header->waitCountersArray());
	// Don't forget to reset it - the next task should not wait on the same counters
	wait_cnt.clear();

	m_impl->last_task_handle = detail::PrivateTaskHandle(header);
	return header->functorStorage();
}

void TaskBuilder::setTaskPayload(TaskPayloadInfo info)
{
	detail::TaskHeader *header = m_impl->last_task_handle.get();
	header->call_fn = info.call;
	header->dtor_fn = info.dtor;
}

void TaskBuilder::doEnqueueTask()
{
	Impl &impl = m_impl.object();
	impl.last_task_counter = impl.service.enqueueTask(std::move(impl.last_task_handle));
}

TaskHandle TaskBuilder::doEnqueueTaskWithHandle()
{
	Impl &impl = m_impl.object();
	impl.last_task_counter = impl.service.enqueueTask(impl.last_task_handle);
	return std::move(impl.last_task_handle);
}

} // namespace voxen::svc
