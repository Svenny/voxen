#pragma once

#include <voxen/common/uid.hpp>
#include <voxen/os/futex.hpp>
#include <voxen/svc/message_types.hpp>

#include <extras/hardware_params.hpp>

#include <deque>
#include <span>
#include <vector>

namespace voxen::svc::detail
{

struct MessageHeader {
	UID from_uid;
	UID msg_uid;
	void (*payload_deleter)(void *) noexcept;
	MessageHeader *queue_link;

	// Call payload deleter and free the pipe memory allocation
	void destroy() noexcept;
	// Payload bytes start right after the header
	void *payload() noexcept { return this + 1; }
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
	// Remove multiple oldest messages, returns the number of removed messages.
	// It will not be greater than `msgs.size()`, remaining pointers are unchanged.
	size_t pop(std::span<MessageHeader *> msgs) noexcept;
	// Drop all messages from the queue, destroying them
	void clear() noexcept;

private:
	os::FutexLock m_lock;
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

	// Registers an agent with given UID, creates an inbound queue and returns pointer to it.
	// Pointer is valid until the next call to `unregisterAgent(id)`, or until router destroys.
	// Throws `Exception` with `VoxenErrc::AlreadyRegistered` if this UID is already registered.
	InboundQueue *registerAgent(UID id);
	// Removes registration and inbound queue of agent with given UID.
	// You cannot use previously returned inbound queue pointer after that.
	void unregisterAgent(UID id) noexcept;
	// Put message `msg` into the inbound queue of agent `to`; drop if the queue is not found
	void send(UID to, MessageHeader *msg) noexcept;

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
