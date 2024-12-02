#include "task_queue_set.hpp"

#include <voxen/os/futex.hpp>
#include <voxen/svc/task_handle.hpp>
#include <voxen/util/log.hpp>

#include "task_handle_private.hpp"

#include <cassert>
#include <chrono>

namespace voxen::svc::detail
{

namespace
{

void onQueueOverflow(size_t queue)
{
	static os::FutexLock s_lock;
	static auto s_last_warn_time = std::chrono::steady_clock::time_point();

	{
		std::lock_guard lock(s_lock);
		auto now = std::chrono::steady_clock::now();

		if (s_last_warn_time == std::chrono::steady_clock::time_point()
			|| now - s_last_warn_time > std::chrono::seconds(5)) {
			s_last_warn_time = now;
			Log::warn("TaskQueueSet: task queue #{} is overflown! Check ring buffer sizes and load distribution.", queue);
			Log::warn("This means slave threads are overwhelmed, and performance will be severely harmed.");
		}
	}

	// Our performance is surely ruined anyway, so we could as well just sleep
	// to throttle task generation and give slave threads some time to catch up.
	//
	// TODO: but the effect will be quite the opposite if we are pushing
	// into our own thread's queue, e.g. a continuation or pinned task.
	//
	// Also see TODO at the call site in `pushTask()`.
	std::this_thread::sleep_for(std::chrono::microseconds(100));
}

} // namespace

TaskQueueSet::TaskQueueSet(size_t num_queues)
	: m_num_queues(num_queues)
	, m_ring_buffer_header(std::make_unique<RingBufferHeader[]>(num_queues))
	, m_ring_buffer_storage(std::make_unique<RingBufferStorage[]>(num_queues))
{
	// Indices are two uint32 packed together, atomics must work like on uint64
	static_assert(std::atomic<ProduceConsumeIndex>::is_always_lock_free);
}

TaskQueueSet::~TaskQueueSet()
{
	for (size_t queue = 0; queue < m_num_queues; queue++) {
		// Deref all remaining stored tasks
		PrivateTaskHandle handle = tryPopTask(queue);
		while (handle.valid()) {
			handle = tryPopTask(queue);
		}
	}
}

void TaskQueueSet::pushTask(size_t queue, PrivateTaskHandle handle) noexcept
{
	// We store raw pointers and assume "nullptr => no data".
	// So pushing in an invalid (null) task handle will blow it up.
	assert(handle.valid());

	auto &header = m_ring_buffer_header[queue];
	auto &storage = m_ring_buffer_storage[queue];

	ProduceConsumeIndex index = header.current_index.load(std::memory_order_relaxed);
	std::atomic<TaskHeader *> *item = nullptr;
	bool need_wake = false;

	while (true) {
		assert(index.produce >= index.consume);
		assert(index.produce <= index.consume + RING_BUFFER_SIZE);

		if (index.consume + RING_BUFFER_SIZE == index.produce) [[unlikely]] {
			// Buffer is full, warn and stall.
			//
			// TODO: I know this is not at all expected and should be solved by
			// adjusting ring buffer sizes or the task generation strategy
			// (i.e. reduce workload, available CPU cores can't keep up with it).
			//
			// But still there are better ways to handle it from the caller side:
			// - Try pushing to other queues, maybe some is not as full. Though if one queue
			//   is overflown then others are most likely close to that too, otherwise
			//   work stealing mechanism should quickly take care of this.
			// - Switch into "assist" mode - pop some tasks off this queue and execute
			//   them as if we're the slave thread. Though it won't help if those tasks
			//   generate more tasks themselves and are just the reason why we got here.
			// - Introduce a separate unbounded "overflow queue", something like a locked deque,
			//   and push this task there. Won't help with sustained task overwhelming but is
			//   good enough to go through short workload bursts without ruining perf completely.
			onQueueOverflow(queue);
			index = header.current_index.load(std::memory_order_relaxed);
			continue;
		}

		item = &storage.item[index.produce % RING_BUFFER_SIZE];
		// Someone waits on this queue - remember it
		need_wake = index.wait_flag;

		// XXX: I'm not sure if this is the most appropriate memory order
		if (item->load(std::memory_order_acquire) != nullptr) [[unlikely]] {
			// Someone has produced this item before us, reload indices and try again
			index = header.current_index.load(std::memory_order_relaxed);
			continue;
		}

		// Try updating the produce index, "reserving" production of this item for our thread
		bool success = header.current_index.compare_exchange_weak(index,
			{
				.produce = index.produce + 1u,
				// Reset the wait flag
				.wait_flag = 0,
				.consume = index.consume,
				// Preserve the stop flag
				.stop_flag = index.stop_flag,
			},
			// XXX: I'm not sure if this is the most appropriate memory order
			std::memory_order_acq_rel);

		if (success) [[likely]] {
			break;
		}
	}

	// `item` is "reserved" for us now - no other push or pop can touch it.
	item->store(handle.release(), std::memory_order_release);
	// Don't forget to wake any thread that could wait for new items
	if (need_wake) {
		// See waiting code in `popTaskOrWait()` to understand this cast
		os::Futex::wakeAll(reinterpret_cast<std::atomic_uint32_t *>(&header.current_index));
	}
}

PrivateTaskHandle TaskQueueSet::tryPopTask(size_t queue) noexcept
{
	auto &header = m_ring_buffer_header[queue];
	auto &storage = m_ring_buffer_storage[queue];

	ProduceConsumeIndex index = header.current_index.load(std::memory_order_relaxed);
	std::atomic<TaskHeader *> *item = nullptr;

	while (true) {
		assert(index.produce >= index.consume);
		assert(index.produce <= index.consume + RING_BUFFER_SIZE);

		if (index.stop_flag) [[unlikely]] {
			// Stop requested
			return {};
		}

		if (index.produce == index.consume) [[unlikely]] {
			// Buffer is empty
			return {};
		}

		item = &storage.item[index.consume % RING_BUFFER_SIZE];

		// XXX: I'm not sure if this is the most appropriate memory order
		if (item->load(std::memory_order_acquire) == nullptr) [[unlikely]] {
			// Someone has taken this item before us, reload indices and try again
			index = header.current_index.load(std::memory_order_relaxed);
			continue;
		}

		// Try updating the consume index, "reserving" consumption of this item for our thread
		bool success = header.current_index.compare_exchange_weak(index,
			{
				.produce = index.produce,
				// Preserve the wait flag
				.wait_flag = index.wait_flag,
				.consume = index.consume + 1u,
				// Preserve the stop flag (must be zero if we reached here)
				.stop_flag = 0,
			},
			// XXX: I'm not sure if this is the most appropriate memory order
			std::memory_order_acq_rel);

		if (success) [[likely]] {
			break;
		}
	};

	// `item` is "reserved" for us now - no other pop or push can touch it.
	// XXX: I'm not sure if this is the most appropriate memory order.
	TaskHeader *task_header = item->exchange(nullptr, std::memory_order_acquire);
	// And assume ownership of the loaded pointer
	return PrivateTaskHandle(task_header);
}

PrivateTaskHandle TaskQueueSet::popTaskOrWait(size_t queue) noexcept
{
	auto &header = m_ring_buffer_header[queue];
	auto &storage = m_ring_buffer_storage[queue];

	ProduceConsumeIndex index = header.current_index.load(std::memory_order_relaxed);
	std::atomic<TaskHeader *> *item = nullptr;

	while (true) {
		assert(index.produce >= index.consume);
		assert(index.produce <= index.consume + RING_BUFFER_SIZE);

		if (index.stop_flag) [[unlikely]] {
			// Stop requested
			return {};
		}

		if (index.produce == index.consume) [[unlikely]] {
			// Buffer is empty - try setting the wait flag and going to sleep
			bool success = header.current_index.compare_exchange_weak(index,
				{
					.produce = index.produce,
					.wait_flag = 1,
					.consume = index.consume,
				},
				// XXX: I'm not sure if this is the most appropriate memory order
				std::memory_order_acq_rel);

			if (!success) [[unlikely]] {
				// Something changed beneath us - try again
				continue;
			}

			// XXX: not the nicest code - take the first uint32 from this struct and wait on it.
			// But futex only supports uint32 words, and it should be indeed enough for us.
			//
			// Produce+wait bit is located in the first word, consume+stop bit in the second.
			// We don't care about the consume index, but stop bit might be a problem.
			// However, stop bit is raised by CAS on both words - and if it notices wait
			// bit in the first word it will properly wake us up, clearing that bit as well.
			//
			// So from futex standpoint it's is totally fine, the only ugly thing is this cast.
			auto words = std::bit_cast<std::array<uint32_t, 2>>(index);
			os::Futex::waitInfinite(reinterpret_cast<std::atomic_uint32_t *>(&header.current_index), words[0]);

			index = header.current_index.load(std::memory_order_relaxed);
			continue;
		}

		item = &storage.item[index.consume % RING_BUFFER_SIZE];

		// XXX: I'm not sure if this is the most appropriate memory order
		if (item->load(std::memory_order_acquire) == nullptr) [[unlikely]] {
			// Someone has taken this item before us, reload indices and try again
			index = header.current_index.load(std::memory_order_relaxed);
			continue;
		}

		// Try updating the consume index, "reserving" consumption of this item for our thread
		bool success = header.current_index.compare_exchange_weak(index,
			{
				.produce = index.produce,
				// Preserve the wait flag
				.wait_flag = index.wait_flag,
				.consume = index.consume + 1u,
				// Preserve the stop flag (must be zero if we reached here)
				.stop_flag = 0,
			},
			// XXX: I'm not sure if this is the most appropriate memory order
			std::memory_order_acq_rel);

		if (success) [[likely]] {
			break;
		}
	};

	// `item` is "reserved" for us now - no other pop or push can touch it.
	// XXX: I'm not sure if this is the most appropriate memory order.
	TaskHeader *task_header = item->exchange(nullptr, std::memory_order_acquire);
	// And assume ownership of the loaded pointer
	return PrivateTaskHandle(task_header);
}

void TaskQueueSet::requestStopAll() noexcept
{
	for (size_t queue = 0; queue < m_num_queues; queue++) {
		auto &header = m_ring_buffer_header[queue];

		ProduceConsumeIndex index = header.current_index.load(std::memory_order_relaxed);
		ProduceConsumeIndex new_index = index;
		bool need_wake = false;

		do {
			need_wake = index.wait_flag;
			new_index.wait_flag = 0;
			new_index.stop_flag = 1;
		} while (!header.current_index.compare_exchange_weak(index, new_index, std::memory_order_release));

		if (need_wake) {
			// See waiting code in `popTaskOrWait()` to understand this cast
			os::Futex::wakeAll(reinterpret_cast<std::atomic_uint32_t *>(&header.current_index));
		}
	}
}

} // namespace voxen::svc::detail
