#pragma once

#include <cstddef>
#include <cstdint>

namespace voxen::msg
{

// Provides tunable constants for messaging subsystem, all in one place
class Config final {
public:
	Config() = delete;

	// Size, in bytes, of a message. Object payloads are stored inline together with control data.
	// This is a hard limit - compilation will fail when trying to pack too big payload.
	// This is a major performance tunable. The larger is it, the bigger payloads can be sent,
	// at the cost of space overhead for small messages. Cache locality can also suffer
	// if too much space is being wasted.
	constexpr static size_t MESSAGE_SIZE = 32;
	// Queue is implemented as a fixed array of lock-protected sub-queues (shards).
	// Array is indexed by hash of thread ID so distinct threads are likely to use
	// distinct shards, thus avoiding lock contention. At the same time all messages
	// from a given thread are delivered in consistent order (it uses only one shard).
	// This is a major performance tunable. The larger is it, the fewer lock contention
	// from different threads is expected, at the cost of memory overhead.
	// It is probably not a good idea to use powers of two or even numbers in general
	// here - thread IDs can be all aligned to a certain power of two.
	constexpr static uint32_t NUM_QUEUE_SHARDS = 5;
	// Each shard of a queue is a forward list of segments - contiguous arrays of messages
	// to reduce pointer chasing overhead. Size of a signle segment should be about
	// the average number of incoming messages per single queue drain - this will provide
	// optimal performance with least memory waste.
	constexpr static uint32_t QUEUE_SEGMENT_SIZE = 32;
};

} // namespace voxen::msg
