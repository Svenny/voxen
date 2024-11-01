#pragma once

#include <voxen/common/uid.hpp>
#include <voxen/os/futex.hpp>
#include <voxen/svc/message_handling.hpp>
#include <voxen/svc/message_types.hpp>

#include <extras/hardware_params.hpp>

#include <deque>
#include <exception>
#include <span>
#include <vector>

namespace voxen::svc::detail
{

struct MessageAuxData {
	// Offset (bytes) from the end of `MessageHeader` to the start of payload
	uint32_t payload_offset : 8 = 0;
	// Whether `MessageDeleterBlock` is added after `MessageHeader`
	uint32_t has_deleter_block : 1 = 0;
	// Whether `MessageRequestBlock` is added after `MessageHeader` or after deleter block
	uint32_t has_request_block : 1 = 0;
	// Can be set only for request-class messages. If set then
	// `MessageRouter::completeRequest` will forward it back as a completion message.
	uint32_t needs_completion_message : 1 = 0;
	// Set when this is a completion message rather than an incoming request.
	// Otherwise there is no difference, completions have the same UID and payload.
	uint32_t is_completion_message : 1 = 0;
	// Unused bits, can become used if we add more features
	uint32_t _padding : 20 = 0;
	// Atomic value for per-message locking, refcounting etc.
	// In current implementation, stores:
	// Bits [15:0] - refcount (initially 1 from header pointer after allocation)
	// Bits [16:16] - futex completion waiting flag (0 - no waiting, 1 - needs waking)
	// Bits [18:17] - value of `RequestStatus` (0 - pending, other values mean complete)
	// Bits [31:19] - unused, must be zero
	std::atomic_uint32_t atomic_word = 1;
};

static_assert(sizeof(MessageAuxData) == sizeof(uint64_t));

struct MessageDeleterBlock {
	// Pointer must be non-zero, otherwise the block shouldn't have been allocated
	void (*deleter)(void *) noexcept;
};

struct MessageRequestBlock {
	// Can store exception thrown by failed request handler function
	std::exception_ptr exception;
};

// This header, all present optional blocks and the payload
// must all be contained within a single pipe memory allocation
struct MessageHeader {
	MessageHeader(bool deleter, bool request) noexcept;

	UID from_uid;
	UID msg_uid;
	MessageHeader *queue_link = nullptr;
	MessageAuxData aux_data = {};

	// Call payload deleter and free the pipe memory allocation
	void releaseRef() noexcept;
	// Get pointer to deleter block (UB if `!aux_data.has_deleter_block`)
	MessageDeleterBlock *deleterBlock() noexcept;
	// Get pointer to request block (UB if `!aux_data.has_request_block`)
	MessageRequestBlock *requestBlock() noexcept;
	// Payload bytes start after the header and optional blocks
	void *payload() noexcept;
};

class alignas(extras::hardware_params::cache_line) InboundQueue {
public:
	InboundQueue() = default;
	InboundQueue(InboundQueue &&) = delete;
	InboundQueue(const InboundQueue &) = delete;
	InboundQueue &operator=(InboundQueue &&) = delete;
	InboundQueue &operator=(const InboundQueue &) = delete;
	~InboundQueue() noexcept { clear(); }

	// Insert message into the queue as the newest using its `queue_link` field.
	// Ownership is acquired, `destroy()` will be called on `clear()`.
	void push(MessageHeader *msg) noexcept;
	// Remove the oldest message from the queue, returns null if queue is empty.
	// Ownership is released, you must call `destroy()` on it.
	MessageHeader *pop() noexcept;
	// Remove multiple oldest messages in order, returns the number of removed messages.
	// It will not be greater than `msgs.size()`, remaining `msgs` items are unchanged.
	size_t pop(std::span<MessageHeader *> msgs) noexcept;
	// Drop all messages from the queue, destroying them.
	void clear() noexcept;

	// Wait for up to `timeout_msec` until any message comes in.
	// Can be called only from one thread (owning the message queue).
	// Returns immediately if there are queued messages.
	// Spurious wake-ups are handled inside, no need to call
	// this in a loop adjusting timeout after each return.
	void wait(uint32_t timeout_msec) noexcept;

private:
	os::FutexLock m_lock;
	std::atomic_uint32_t m_wait_word = 0;
	MessageHeader *m_oldest = nullptr;
	MessageHeader *m_newest = nullptr;
};

// A component of `MessageRouter`, usually should not be used directly
class RoutingShard {
public:
	// Returns inbound queue for `id` or null if it is not recorded
	InboundQueue *findRoute(UID id) noexcept;
	// Records inbound queue `q` for `id` and returns true if it's not yet registered.
	// Returns false and does nothing otherwise.
	bool addRoute(UID id, InboundQueue *q);
	// Removes inbound queue record for `id` and returns that queue (null if not recorded)
	InboundQueue *removeRoute(UID id) noexcept;

private:
	using Route = std::pair<UID, InboundQueue *>;

	// Protects access to `m_routes`
	os::FutexRWLock m_lock;
	// Maps registered agent UIDs to their inbound queues.
	// Sorted array of agent UID => his inbound queue mappings.
	// Slow insertions but quite fast and cache-efficient lookups.
	std::vector<Route> m_routes;

	static bool routeComparator(const Route &a, const Route &b) noexcept { return a.first < b.first; }
};

// Routes UIDs to inbound message queues
class MessageRouter {
public:
	// We want many, many shards to freely use fine-grained
	// locking with little chances of any contention.
	// TODO: move to some more centralized constants storage?
	constexpr static uint64_t NUM_SHARDS = 512;

	// Register an agent with given UID, create an inbound queue and return pointer to it.
	// Pointer is valid until the next call to `unregisterAgent(id)`, or until the router destroys.
	// Throws `Exception` with `VoxenErrc::AlreadyRegistered` if this UID is already registered.
	InboundQueue *registerAgent(UID id);
	// Remove registration and inbound queue of agent with given UID.
	// You cannot use previously returned inbound queue pointer after that.
	void unregisterAgent(UID id) noexcept;
	// Put message `msg` into the inbound queue of agent `to`; drop if the queue is not found.
	// You disown the pointer after this call, don't release ref manually.
	void send(UID to, MessageHeader *msg) noexcept;
	// Mark request as complete with `status`, which must be not `Pending`,
	// waking up the waiting sender and/or forwarding completion message as needed.
	// You disown the pointer after this call, don't release ref manually.
	void completeRequest(MessageHeader *msg, RequestStatus status) noexcept;

	// Every UID belongs to one shard
	RoutingShard &getShard(UID id) noexcept { return m_shards[id.v1 % NUM_SHARDS]; }

private:
	RoutingShard m_shards[NUM_SHARDS];
	// Stores inbound queue objects.
	// Deque can insert elements without invalidating references
	// so we can give away raw pointers while adding new queues.
	std::deque<InboundQueue> m_queue_storage;
	// Queues from `m_queue_storage` not assigned to any agent, available for reuse
	std::vector<InboundQueue *> m_free_queues;
	// This lock protects access to `m_queue_storage` and `m_free_queues`.
	// Placed at the end to be surely separated from locks in `m_shards`.
	os::FutexLock m_queues_lock;
};

} // namespace voxen::svc::detail
