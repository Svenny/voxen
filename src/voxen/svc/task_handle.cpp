#include <voxen/svc/task_handle.hpp>

#include <voxen/common/pipe_memory_allocator.hpp>
#include <voxen/os/futex.hpp>

#include "task_counter_tracker.hpp"
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

void doResetRef(detail::TaskHeader *header) noexcept
{
	if ((header->atomic_word.fetch_sub(1, std::memory_order_acq_rel) & ATOMIC_WORD_REFCOUNT_MASK) > 1) {
		// This was not the last ref
		return;
	}

	if (header->dtor_fn) [[likely]] {
		// Dtor might be missing if this header was not fully constructed in `TaskBuilder`,
		// otherwise it's quite rare (trivially destructible functor storage optimization?)
		header->dtor_fn(header->functorStorage());
	}

	header->~TaskHeader();
	PipeMemoryAllocator::deallocate(header);
}

void doComplete(detail::TaskHeader *header, detail::TaskCounterTracker &tracker)
{
	// First complete the parent task, if any
	header->parent_handle.onTaskComplete(tracker);

	// Set completion flag in the task header.
	// Not waking immediately, see comments below.
	bool need_wake = !!(header->atomic_word.fetch_or(ATOMIC_WORD_FINISHED_BIT, std::memory_order_release)
		& ATOMIC_WORD_FUTEX_WAITING_BIT);

	// Mark task counter as complete to unblock scheduling of dependent tasks.
	// Note we're doing this only after the completion flag is raised. It must be
	// formally complete from `TaskHandle` point of view before dependants begin.
	tracker.completeCounter(header->task_counter);

	// After we wake a waiting thread it can start enqueueing new tasks immediately
	// while the task counter is not yet completed - there is a tiny gap between those
	// two actions and this thread could get un-scheduled there. It's not a problem.
	//
	// But if we do wake immediately after setting the flag then we're going to
	// extend this gap by the syscall duration, and dependent tasks will not
	// start executing during that gap. So complete the counter first, then wake.
	if (need_wake) [[unlikely]] {
		// Someone is waiting for task completion.
		// We don't know how many (task handle can be shared) so wake all.
		os::Futex::wakeAll(&header->atomic_word);
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

void detail::ParentTaskHandle::onTaskComplete(TaskCounterTracker &tracker)
{
	// Tasks are expected to be mostly indepent (not continuations)
	if (!m_parent) [[likely]] {
		return;
	}

	uint32_t old_word = m_parent->atomic_word.fetch_sub(ATOMIC_WORD_CONTINUATION_ADD, std::memory_order_acq_rel);
	if ((old_word & ATOMIC_WORD_CONTINUATION_COUNT_MASK) == ATOMIC_WORD_CONTINUATION_ADD) {
		// This was the last continuation, complete the parent
		doComplete(m_parent, tracker);
	}

	// Reset ref - can't to it in the same atomic operation,
	// we need to hold this ref during completion signaling.
	doResetRef(m_parent);
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

	doResetRef(m_header);
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

void detail::PrivateTaskHandle::complete(TaskCounterTracker &tracker)
{
	doComplete(m_header, tracker);
}

} // namespace voxen::svc
