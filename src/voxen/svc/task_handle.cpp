#include <voxen/svc/task_handle.hpp>

#include <voxen/common/pipe_memory_allocator.hpp>
#include <voxen/os/futex.hpp>

#include "async_counter_tracker.hpp"
#include "task_handle_private.hpp"

#include <cassert>

namespace voxen::svc
{

namespace
{

constexpr uint32_t ATOMIC_WORD_REFCOUNT_MASK = (1u << 16) - 1u;
constexpr uint32_t ATOMIC_WORD_FUTEX_WAITING_BIT = 1u << 16;
constexpr uint32_t ATOMIC_WORD_FINISHED_BIT = 1u << 17;
constexpr uint32_t ATOMIC_WORD_CONTINUATION_COUNT_MASK = ((1u << 12) - 1u) << 20;
constexpr uint32_t ATOMIC_WORD_CONTINUATION_ADD = 1u << 20;
// Adds 1 to both refcount and continuation count
constexpr uint32_t ATOMIC_WORD_CONTINUATION_REF_ADD = ATOMIC_WORD_CONTINUATION_ADD | 1u;

void doReleaseRef(detail::TaskHeader *header) noexcept
{
	if ((header->atomic_word.fetch_sub(1, std::memory_order_acq_rel) & ATOMIC_WORD_REFCOUNT_MASK) > 1) {
		// This was not the last ref
		return;
	}

	header->~TaskHeader();
	PipeMemoryAllocator::deallocate(header);
}

void doCompleteAndUnref(detail::TaskHeader *header, detail::AsyncCounterTracker &tracker)
{
	// First complete the parent task, if any
	header->parent_handle.onTaskComplete(tracker);

	// Set completion flag in the task header.
	// Not waking immediately, see comments below.
	bool need_wake = !!(header->atomic_word.fetch_or(ATOMIC_WORD_FINISHED_BIT, std::memory_order_release)
		& ATOMIC_WORD_FUTEX_WAITING_BIT);

	const uint64_t task_counter = header->task_counter;
	std::atomic_uint32_t *atomic_word = &header->atomic_word;

	// Release ref before marking the counter as complete.
	// If this was the last ref to this task then its associated resources
	// will be freed by the time any waiting thread unblocks. This is
	// needed e.g. for subsystem destructors that wait for all
	// enqueued tasks to finish using only task counters, not handles.
	doReleaseRef(header);

	// Mark task counter as complete to unblock scheduling of dependent tasks.
	// Note we're doing this only after the completion flag is raised. It must be
	// formally complete from `TaskHandle` point of view before dependants begin.
	tracker.completeCounter(task_counter);

	// After we wake a waiting thread it can start enqueueing new tasks immediately
	// while the task counter is not yet completed - there is a small gap between those
	// two actions and this thread could get un-scheduled there. It's not a problem.
	//
	// But if we do wake immediately after setting the flag then we're going to
	// extend this gap by the syscall duration, and dependent tasks will not
	// start executing during that gap. So complete the counter first, then wake.
	if (need_wake) [[unlikely]] {
		// Someone is waiting for task completion.
		// We don't know how many (task handle can be shared) so wake all.
		//
		// XXX: we assume there is at least one more ref to `header` remaining,
		// otherwise we would wake on a no longer valid address. Could fuse `fetch_or`
		// and unref into a single atomic operation and check that more refs remain.
		os::Futex::wakeAll(atomic_word);
	}
}

} // namespace

detail::ParentTaskHandle::~ParentTaskHandle()
{
	// If it's not null then we're in shit - parent counter will never complete
	assert(!m_parent);
}

void detail::ParentTaskHandle::setParent(TaskHeader *header) noexcept
{
	assert(!m_parent);
	m_parent = header;

	if (header) [[likely]] {
		header->atomic_word.fetch_add(ATOMIC_WORD_CONTINUATION_REF_ADD, std::memory_order_relaxed);
	}
}

void detail::ParentTaskHandle::onTaskComplete(AsyncCounterTracker &tracker)
{
	// Tasks are expected to be mostly indepent (not continuations)
	if (!m_parent) [[likely]] {
		return;
	}

	uint32_t old_word = m_parent->atomic_word.fetch_sub(ATOMIC_WORD_CONTINUATION_ADD, std::memory_order_acq_rel);
	if ((old_word & ATOMIC_WORD_CONTINUATION_COUNT_MASK) == ATOMIC_WORD_CONTINUATION_ADD) {
		// This was the last continuation, complete the parent
		doCompleteAndUnref(m_parent, tracker);
	} else {
		// Parent has more incomplete continuations, but we no longer need to ref it
		doReleaseRef(m_parent);
	}

	m_parent = nullptr;
}

TaskHandle::TaskHandle(detail::PrivateTaskHandle &&handle) noexcept : m_header(std::exchange(handle.m_header, nullptr))
{}

TaskHandle::TaskHandle(const TaskHandle &other) noexcept : m_header(other.m_header)
{
	addRef();
}

TaskHandle &TaskHandle::operator=(const TaskHandle &other) noexcept
{
	if (m_header != other.m_header) {
		reset();
		m_header = other.m_header;
		addRef();
	}

	return *this;
}

TaskHandle::~TaskHandle()
{
	reset();
}

void TaskHandle::reset() noexcept
{
	if (!m_header) {
		return;
	}

	doReleaseRef(m_header);
	m_header = nullptr;
}

bool TaskHandle::finished() const noexcept
{
	assert(m_header);
	return !!(m_header->atomic_word.load(std::memory_order_acquire) & ATOMIC_WORD_FINISHED_BIT);
}

void TaskHandle::wait() noexcept
{
	assert(m_header);

	uint32_t word = m_header->atomic_word.load(std::memory_order_acquire);
	if (word & ATOMIC_WORD_FINISHED_BIT) {
		// Already finished
		return;
	}

	// Set futex waiting flag. We don't need to care about resetting it,
	// it will be taken into consideration just once in `doComplete()`.
	word = m_header->atomic_word.fetch_or(ATOMIC_WORD_FUTEX_WAITING_BIT, std::memory_order_release);

	while (!(word & ATOMIC_WORD_FINISHED_BIT)) {
		os::Futex::waitInfinite(&m_header->atomic_word, word);
		// Reload value after returning from wait
		word = m_header->atomic_word.load(std::memory_order_acquire);
	}
}

uint64_t TaskHandle::getCounter() const noexcept
{
	return m_header ? m_header->task_counter : 0;
}

void TaskHandle::addRef() noexcept
{
	if (m_header) {
		m_header->atomic_word.fetch_add(1, std::memory_order_relaxed);
	}
}

bool detail::PrivateTaskHandle::hasContinuations() const noexcept
{
	return !!(m_header->atomic_word.load(std::memory_order_acquire) & ATOMIC_WORD_CONTINUATION_COUNT_MASK);
}

void detail::PrivateTaskHandle::completeAndReset(AsyncCounterTracker &tracker)
{
	doCompleteAndUnref(m_header, tracker);
	m_header = nullptr;
}

} // namespace voxen::svc
