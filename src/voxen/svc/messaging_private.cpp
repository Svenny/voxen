#include "messaging_private.hpp"

#include <voxen/common/pipe_memory_allocator.hpp>
#include <voxen/debug/debug_uid_registry.hpp>
#include <voxen/util/error_condition.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

#include <extras/defer.hpp>

#include <mutex>
#include <shared_mutex>
#include <utility>

namespace voxen::svc::detail
{

static_assert(std::is_trivially_destructible_v<MessageHeader>);

void MessageHeader::destroy() noexcept
{
	if (payload_deleter) {
		payload_deleter(payload());
	}

	PipeMemoryAllocator::deallocate(this);
}

// --- InboundQueue ---

void InboundQueue::push(MessageHeader *hdr) noexcept
{
	hdr->queue_link = nullptr;

	std::lock_guard lk(m_lock);

	if (!m_newest) {
		// Empty queue
		m_newest = hdr;
		m_oldest = hdr;
		return;
	}

	// Non-empty queue
	m_newest->queue_link = hdr;
	m_newest = hdr;
}

MessageHeader *InboundQueue::pop() noexcept
{
	std::lock_guard lk(m_lock);

	if (!m_oldest) {
		// Empty queue
		return nullptr;
	}

	MessageHeader *msg = std::exchange(m_oldest, m_oldest->queue_link);

	if (!m_oldest) {
		// Just popped the last element
		m_newest = nullptr;
	}

	return msg;
}

size_t InboundQueue::pop(std::span<MessageHeader *> msgs) noexcept
{
	std::lock_guard lk(m_lock);

	size_t popped = 0;
	while (popped < msgs.size()) {
		if (!m_oldest) {
			break;
		}

		msgs[popped] = std::exchange(m_oldest, m_oldest->queue_link);
		popped++;
	}

	if (!m_oldest) {
		// Popped the last element
		m_newest = nullptr;
	}

	return popped;
}

void InboundQueue::clear() noexcept
{
	MessageHeader *msg = pop();
	while (msg) {
		msg->destroy();
		msg = pop();
	}
}

// --- RoutingShard ---

InboundQueue *RoutingShard::findRoute(UID id) noexcept
{
	// Shared lock - we're only reading
	std::shared_lock lk(m_lock);

	Route dummy(id, nullptr);
	auto iter = std::lower_bound(m_routes.begin(), m_routes.end(), dummy, routeComparator);
	return (iter != m_routes.end() && iter->first == id) ? iter->second : nullptr;
}

bool RoutingShard::addRoute(UID id, InboundQueue *q)
{
	// Exclusive lock - we're writing
	std::lock_guard lk(m_lock);

	Route item(id, q);
	auto iter = std::lower_bound(m_routes.begin(), m_routes.end(), item, routeComparator);
	if (iter != m_routes.end() && iter->first == id) {
		return false;
	}

	m_routes.insert(iter, item);
	return true;
}

InboundQueue *RoutingShard::removeRoute(UID id) noexcept
{
	// Exclusive lock - we're writing
	std::lock_guard lk(m_lock);

	Route dummy(id, nullptr);
	auto iter = std::lower_bound(m_routes.begin(), m_routes.end(), dummy, routeComparator);
	if (iter == m_routes.end() || iter->first != id) {
		return nullptr;
	}

	InboundQueue *q = iter->second;
	m_routes.erase(iter);
	return q;
}

// --- MessageRouter ---

InboundQueue *MessageRouter::registerAgent(UID id)
{
	RoutingShard &shard = getShard(id);

	std::lock_guard lk(m_queues_lock);

	InboundQueue *q = nullptr;

	if (!m_free_queues.empty()) {
		// Reuse a queue from the free list
		q = m_free_queues.back();
		m_free_queues.pop_back();
	} else {
		// No free queues, create a new one
		q = &m_queue_storage.emplace_back();
	}

	if (!shard.addRoute(id, q)) {
		// Already routed, return the queue to the free list
		m_free_queues.emplace_back(q);

		Log::error("Messaging agent {} is already registered!", debug::UidRegistry::lookup(id));
		throw Exception::fromError(VoxenErrc::AlreadyRegistered, "double-registration of messaging agent");
	}

	return q;
}

void MessageRouter::unregisterAgent(UID id) noexcept
{
	RoutingShard &shard = getShard(id);
	InboundQueue *q = shard.removeRoute(id);

	if (q != nullptr) {
		// Clear this queue of any remaining messages and place into the free list
		q->clear();

		std::lock_guard lk(m_queues_lock);
		m_free_queues.emplace_back(q);
	}
}

void MessageRouter::send(UID to, MessageHeader *msg) noexcept
{
	InboundQueue *q = getShard(to).findRoute(to);
	if (q) {
		q->push(msg);
	} else {
		// No recipient, drop the message
		msg->destroy();
	}
}

} // namespace voxen::svc::detail
