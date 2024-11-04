#include <voxen/svc/message_queue.hpp>

#include <voxen/common/pipe_memory_allocator.hpp>

#include "messaging_private.hpp"

#include <extras/defer.hpp>

#include <algorithm>
#include <array>

namespace voxen::svc
{

using detail::InboundQueue;
using detail::MessageHeader;
using detail::MessageRouter;

struct MessageQueue::Impl {
	Impl() = default;
	Impl(Impl &&other) noexcept { *this = std::move(other); }
	Impl(const Impl &) = delete;

	Impl &operator=(Impl &&other) noexcept
	{
		std::swap(inbound_queue, other.inbound_queue);
		std::swap(handlers, other.handlers);
		std::swap(completion_handlers, other.completion_handlers);
		return *this;
	}

	Impl &operator=(const Impl &) = delete;
	~Impl() = default;

	InboundQueue *inbound_queue = nullptr;
	// Sorted array of message UID => handler functions.
	// Slow insertions but quite fast and cache-efficient lookups.
	// TODO: use something with even faster/simpler lookups? Semi-perfect hashing?
	std::vector<HandlerItem> handlers;
	// Same for completion handlers
	std::vector<CompletionHandlerItem> completion_handlers;
};

MessageQueue::MessageQueue() noexcept = default;

MessageQueue::MessageQueue(MessageRouter &router, UID my_uid) : MessageSender(router, my_uid)
{
	m_impl->inbound_queue = router.registerAgent(my_uid);
}

MessageQueue::MessageQueue(MessageQueue &&other) noexcept
	: MessageSender(std::move(other)), m_impl(std::move(other.m_impl))
{}

MessageQueue &MessageQueue::operator=(MessageQueue &&other) noexcept
{
	MessageSender::operator=(std::move(other));
	std::swap(m_impl.object(), other.m_impl.object());
	return *this;
}

MessageQueue::~MessageQueue() noexcept
{
	if (m_impl->inbound_queue) {
		m_router->unregisterAgent(m_my_uid);
	}
}

void MessageQueue::pollMessages()
{
	Impl &impl = m_impl.object();

	while (true) {
		// Pop messages in small batches to take locks less often.
		// TODO: make pop batch size tuneable?
		std::array<MessageHeader *, 8> hdrs;
		size_t popped = impl.inbound_queue->pop(hdrs);

		if (popped == 0) {
			return;
		}

		for (size_t i = 0; i < popped; i++) {
			MessageHeader *hdr = hdrs[i];

			if (hdr->aux_data.is_completion_message) {
				// Completion message, handle it specially.
				// Release ref even if the handler throws
				defer { hdr->releaseRef(); };

				CompletionHandlerItem dummy_item(hdr->msg_uid, CompletionHandler());
				auto handler_iter = std::lower_bound(impl.completion_handlers.begin(), impl.completion_handlers.end(),
					dummy_item, completionHandlerComparator);
				if (handler_iter != impl.completion_handlers.end() && handler_iter->first == hdr->msg_uid) {
					RequestCompletionInfo info(hdr);
					handler_iter->second(info, hdr->payload());
				}

				continue;
			}

			HandlerItem dummy_item(hdr->msg_uid, MessageHandler());
			auto handler_iter = std::lower_bound(impl.handlers.begin(), impl.handlers.end(), dummy_item,
				handlerComparator);

			bool handler_valid = handler_iter != impl.handlers.end() && handler_iter->first == hdr->msg_uid;

			if (hdr->aux_data.has_request_block) {
				// Request message, handle it specially
				if (handler_valid) {
					try {
						MessageInfo info(hdr);
						handler_iter->second(info, hdr->payload());
						m_router->completeRequest(hdr, RequestStatus::Complete);
					}
					catch (...) {
						hdr->requestBlock()->exception = std::current_exception();
						m_router->completeRequest(hdr, RequestStatus::Failed);
					}
				} else {
					m_router->completeRequest(hdr, RequestStatus::Dropped);
				}
			} else {
				// Release ref even if the handler throws
				defer{ hdr->releaseRef(); };

				// Non-request message
				if (handler_valid) {
					MessageInfo info(hdr);
					handler_iter->second(info, hdr->payload());
				}
			}
		}
	}
}

void MessageQueue::waitMessages(uint32_t timeout_msec)
{
	Impl &impl = m_impl.object();
	impl.inbound_queue->wait(timeout_msec);
	pollMessages();
}

bool MessageQueue::handlerComparator(const HandlerItem &a, const HandlerItem &b) noexcept
{
	return a.first < b.first;
}

bool MessageQueue::completionHandlerComparator(const CompletionHandlerItem &a, const CompletionHandlerItem &b) noexcept
{
	return a.first < b.first;
}

void MessageQueue::doRequestWithCompletion(UID to, UID msg_uid, MessageHeader *header, PayloadDeleter deleter)
{
	header->aux_data.needs_completion_message = 1;
	doSend(to, msg_uid, header, deleter);
}

void MessageQueue::doRegisterHandler(UID msg_uid, MessageHandler handler)
{
	HandlerItem item(msg_uid, std::move(handler));
	auto iter = std::lower_bound(m_impl->handlers.begin(), m_impl->handlers.end(), item, handlerComparator);
	if (iter != m_impl->handlers.end() && iter->first == msg_uid) {
		*iter = std::move(item);
	} else {
		m_impl->handlers.insert(iter, std::move(item));
	}
}

void MessageQueue::doRegisterCompletionHandler(UID msg_uid, CompletionHandler handler)
{
	CompletionHandlerItem item(msg_uid, std::move(handler));
	auto iter = std::lower_bound(m_impl->completion_handlers.begin(), m_impl->completion_handlers.end(), item,
		completionHandlerComparator);
	if (iter != m_impl->completion_handlers.end() && iter->first == msg_uid) {
		*iter = std::move(item);
	} else {
		m_impl->completion_handlers.insert(iter, std::move(item));
	}
}

void MessageQueue::doUnregisterHandler(UID msg_uid) noexcept
{
	HandlerItem dummy_item(msg_uid, MessageHandler());
	auto iter = std::lower_bound(m_impl->handlers.begin(), m_impl->handlers.end(), dummy_item, handlerComparator);
	if (iter != m_impl->handlers.end() && iter->first == msg_uid) {
		m_impl->handlers.erase(iter);
	}
}

void MessageQueue::doUnregisterCompletionHandler(UID msg_uid) noexcept
{
	CompletionHandlerItem dummy_item(msg_uid, CompletionHandler());
	auto iter = std::lower_bound(m_impl->completion_handlers.begin(), m_impl->completion_handlers.end(), dummy_item,
		completionHandlerComparator);
	if (iter != m_impl->completion_handlers.end() && iter->first == msg_uid) {
		m_impl->completion_handlers.erase(iter);
	}
}

} // namespace voxen::svc
