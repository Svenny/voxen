#pragma once

#include <voxen/svc/svc_fwd.hpp>

#include <extras/hardware_params.hpp>

#include <atomic>
#include <memory>

namespace voxen::svc::detail
{

class TaskQueueSet {
public:
	// Must be a power of two for two reasons:
	// - Trivial modulo operation (masking off lower bits)
	// - So that wraparound of `ProduceConsumeIndex` does not cause troubles
	constexpr static uint64_t RING_BUFFER_SIZE = 1024;

	TaskQueueSet(size_t num_queues);
	TaskQueueSet(TaskQueueSet &&) = delete;
	TaskQueueSet(const TaskQueueSet &) = delete;
	TaskQueueSet &operator=(TaskQueueSet &&) = delete;
	TaskQueueSet &operator=(const TaskQueueSet &) = delete;
	~TaskQueueSet();

	void pushTask(size_t queue, PrivateTaskHandle handle) noexcept;

	PrivateTaskHandle tryPopTask(size_t queue) noexcept;
	PrivateTaskHandle popTaskOrWait(size_t queue) noexcept;

	void requestStopAll() noexcept;

private:
	struct alignas(uint64_t) ProduceConsumeIndex {
		// Number of produced (pushed) items, wraparound is fine
		uint32_t produce : 31 = 0;
		// Set when one or more threads are waiting on this queue
		uint32_t wait_flag : 1 = 0;
		// Number of consumed (popped) items, wraparound is fine
		uint32_t consume : 31 = 0;
		// Set when threads attached to this queue are requested to stop
		uint32_t stop_flag : 1 = 0;
	};

	struct alignas(extras::hardware_params::cache_line) RingBufferHeader {
		std::atomic<ProduceConsumeIndex> current_index;
	};

	struct alignas(extras::hardware_params::cache_line) RingBufferStorage {
		std::atomic<TaskHeader *> item[RING_BUFFER_SIZE];
	};

	const size_t m_num_queues;
	std::unique_ptr<RingBufferHeader[]> m_ring_buffer_header;
	std::unique_ptr<RingBufferStorage[]> m_ring_buffer_storage;
};

} // namespace voxen::svc::detail
