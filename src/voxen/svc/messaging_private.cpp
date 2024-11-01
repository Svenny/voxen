#include "messaging_private.hpp"

#include <voxen/common/pipe_memory_allocator.hpp>
#include <voxen/debug/debug_uid_registry.hpp>
#include <voxen/util/error_condition.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

#include <extras/defer.hpp>

#include <atomic>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <utility>

namespace voxen::svc::detail
{

static_assert(std::is_trivially_destructible_v<MessageHeader>);
static_assert(std::is_trivially_destructible_v<MessageDeleterBlock>);
static_assert(std::is_nothrow_destructible_v<MessageRequestBlock>);

MessageHeader::MessageHeader(bool deleter, bool request) noexcept
{
	aux_data.has_deleter_block = deleter;
	aux_data.has_request_block = request;

	uint32_t payload_offset = 0;
	if (deleter) {
		payload_offset += sizeof(MessageDeleterBlock);
		new (deleterBlock()) MessageDeleterBlock;
	}

	if (request) {
		payload_offset += sizeof(MessageRequestBlock);
		new (requestBlock()) MessageRequestBlock;
	}

	aux_data.payload_offset = payload_offset;
}

void MessageHeader::releaseRef() noexcept
{
	// Refcount is stored in low 16 bits
	if ((aux_data.atomic_word.fetch_sub(1, std::memory_order_acq_rel) & 0xFFFF) == 1) {
		// Released the last reference, delete it

		if (aux_data.has_deleter_block) {
			deleterBlock()->deleter(payload());
		}

		if (aux_data.has_request_block) {
			// Request block is not trivially destructible
			requestBlock()->~MessageRequestBlock();
		}

		PipeMemoryAllocator::deallocate(this);
	}
}

MessageDeleterBlock *MessageHeader::deleterBlock() noexcept
{
	// Deleter block is always the first optional block
	return reinterpret_cast<MessageDeleterBlock *>(this + 1);
}

MessageRequestBlock *MessageHeader::requestBlock() noexcept
{
	// Request block is the second optional block (after deleter)
	if (aux_data.has_deleter_block) {
		return reinterpret_cast<MessageRequestBlock *>(reinterpret_cast<MessageDeleterBlock *>(this + 1) + 1);
	}
	return reinterpret_cast<MessageRequestBlock *>(this + 1);
}

void *MessageHeader::payload() noexcept
{
	uintptr_t ptr = reinterpret_cast<uintptr_t>(this + 1) + aux_data.payload_offset;
	return reinterpret_cast<void *>(ptr);
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
	} else {
		// Non-empty queue
		m_newest->queue_link = hdr;
		m_newest = hdr;
	}

	// Notify waiting thread that messages have arrived.
	// Relaxed order - no extra sync is needed inside critical section.
	if (m_wait_word.exchange(0, std::memory_order_relaxed) == 1) {
		// Note - waking while still holding a lock.
		// It eliminates any chance of double wake-up.
		os::Futex::wakeSingle(&m_wait_word);
	}
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

	uint32_t popped = 0;
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
	std::lock_guard lk(m_lock);

	while (m_oldest) {
		MessageHeader *msg = std::exchange(m_oldest, m_oldest->queue_link);
		msg->releaseRef();
	}

	m_newest = nullptr;
}

void InboundQueue::wait(uint32_t timeout_msec) noexcept
{
	auto time_point_now = std::chrono::steady_clock::now();
	auto target_time_point = time_point_now + std::chrono::milliseconds(timeout_msec);

	while (true) {
		// No `lock_guard` - be careful
		m_lock.lock();

		if (m_oldest != nullptr) {
			// Already got some messages
			m_lock.unlock();
			return;
		}

		// Check if the timeout has expired.
		// Update time point as taking lock could take some time.
		time_point_now = std::chrono::steady_clock::now();
		if (time_point_now >= target_time_point) {
			// Timeout expired
			m_lock.unlock();
			return;
		}

		// Set waiting flag. Note that we should hold the lock,
		// otherwise a pushing thread can miss this value.
		// Relaxed order - no extra sync is needed inside critical section.
		m_wait_word.store(1, std::memory_order_relaxed);
		// Note - dropping the lock just before waiting
		m_lock.unlock();

		auto time_diff = target_time_point - time_point_now;
		auto timeout = std::chrono::duration_cast<std::chrono::duration<uint32_t, std::milli>>(time_diff);
		// Wait until it is reset back to zero by a pushing thread.
		// As we're not holding the lock it can happen right before entering
		// the function - that's ok, then it will return immediately.
		os::Futex::waitFor(&m_wait_word, 1, timeout.count());
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
		if (msg->aux_data.has_request_block) {
			completeRequest(msg, RequestStatus::Dropped);
		} else {
			msg->releaseRef();
		}
	}
}

void MessageRouter::completeRequest(MessageHeader *msg, RequestStatus status) noexcept
{
	constexpr uint32_t WAIT_BIT = 1u << 16;

	// Convert to bitmask (bits [18:17]) and write it with OR (there were zeros before)
	uint32_t mask = extras::to_underlying(status) << 17;
	uint32_t word = msg->aux_data.atomic_word.fetch_or(mask, std::memory_order_acq_rel);

	if (word & WAIT_BIT) {
		// Sender waits on completion, wake him up.
		// Clearing wait flag is not necessary - there will be no more waits.
		os::Futex::wakeSingle(&msg->aux_data.atomic_word);
	}

	if (msg->aux_data.needs_completion_message) {
		// Sender wants completion message, forward it back to him
		msg->aux_data.is_completion_message = 1;

		InboundQueue *q = getShard(msg->from_uid).findRoute(msg->from_uid);
		if (q) {
			q->push(msg);
			return;
		}
	}

	// Message is no longer needed or could not be forwarded back, drop it.
	// No need to change request status to `Dropped` in this case.
	msg->releaseRef();
}

} // namespace voxen::svc::detail
