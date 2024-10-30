#include <voxen/svc/message_queue.hpp>

#include <voxen/common/pipe_memory_allocator.hpp>

#include "messaging_private.hpp"

#include <algorithm>
#include <array>

namespace voxen::svc
{

using detail::MessageHeader;
using detail::MessageRouter;

struct MessageQueue::Impl {
	Impl() = default;
	Impl(MessageRouter &router, UID &my_uid) : router(&router), my_uid(my_uid) {}
	Impl(Impl &&other) = default;
	Impl(const Impl &) = delete;
	Impl &operator=(Impl &&other) = default;
	Impl &operator=(const Impl &) = delete;
	~Impl() = default;

	MessageRouter *router = nullptr;
	UID my_uid;
	// Sorted array of message UID => handler functions.
	// Slow insertions but quite fast and cache-efficient lookups.
	// TODO: use something with even faster/simpler lookups? Semi-perfect hashing?
	std::vector<HandlerItem> handlers;

	detail::InboundQueue *inbound_queue = nullptr;
};

MessageQueue::MessageQueue() noexcept = default;

MessageQueue::MessageQueue(MessageRouter &router, UID my_uid) : m_impl(router, my_uid)
{
	m_impl->inbound_queue = router.registerAgent(my_uid);
}

MessageQueue::MessageQueue(MessageQueue &&other) noexcept : m_impl(std::move(other.m_impl)) {}

MessageQueue &MessageQueue::operator=(MessageQueue &&other) noexcept
{
	std::swap(m_impl.object(), other.m_impl.object());
	return *this;
}

MessageQueue::~MessageQueue() noexcept
{
	if (m_impl->router) {
		m_impl->router->unregisterAgent(m_impl->my_uid);
	}
}

void MessageQueue::receiveAll()
{
	Impl &impl = m_impl.object();

	while (true) {
		std::array<MessageHeader *, 8> hdrs;
		size_t popped = impl.inbound_queue->pop(hdrs);

		if (popped == 0) {
			return;
		}

		for (size_t i = 0; i < popped; i++) {
			MessageHeader *hdr = hdrs[i];

			std::pair<UID, MessageHandler> dummy_item(hdr->msg_uid, MessageHandler());
			auto handler_iter = std::lower_bound(impl.handlers.begin(), impl.handlers.end(), dummy_item,
				handlerComparator);

			if (handler_iter != impl.handlers.end() && handler_iter->first == hdr->msg_uid) {
				MessageInfo info(hdr);
				handler_iter->second(info, hdr->payload());
			}

			hdr->destroy();
		}
	}
}

std::pair<void *, void *> MessageQueue::allocatePayloadStorage(size_t size)
{
	static_assert(alignof(MessageHeader) <= alignof(void *), "Payload header is over-aligned");
	static_assert(sizeof(MessageHeader) % alignof(void *) == 0, "Payload start is not properly aligned");

	void *alloc = PipeMemoryAllocator::allocate(sizeof(MessageHeader) + size, alignof(void *));
	uintptr_t payload = reinterpret_cast<uintptr_t>(alloc) + sizeof(MessageHeader);
	return { alloc, reinterpret_cast<void *>(payload) };
}

void MessageQueue::freePayloadStorage(void *alloc) noexcept
{
	PipeMemoryAllocator::deallocate(alloc);
}

bool MessageQueue::handlerComparator(const HandlerItem &a, const HandlerItem &b) noexcept
{
	return a.first < b.first;
}

void MessageQueue::doSend(UID to, UID msg_uid, void *alloc, PayloadDeleter deleter)
{
	MessageHeader *hdr = new (alloc) MessageHeader {
		.from_uid = m_impl->my_uid,
		.msg_uid = msg_uid,
		.payload_deleter = deleter,
		.queue_link = nullptr,
	};

	m_impl->router->send(to, hdr);
}

void MessageQueue::doRegisterHandler(UID msg_uid, MessageHandler handler)
{
	std::pair<UID, MessageHandler> item(msg_uid, std::move(handler));
	auto iter = std::upper_bound(m_impl->handlers.begin(), m_impl->handlers.end(), item, handlerComparator);
	m_impl->handlers.insert(iter, std::move(item));
}

void MessageQueue::doUnregisterHandler(UID msg_uid) noexcept
{
	std::pair<UID, MessageHandler> dummy_item(msg_uid, MessageHandler());
	auto iter = std::lower_bound(m_impl->handlers.begin(), m_impl->handlers.end(), dummy_item, handlerComparator);
	if (iter != m_impl->handlers.end() && iter->first == msg_uid) {
		m_impl->handlers.erase(iter);
	}
}

} // namespace voxen::svc
