#include <voxen/svc/message_sender.hpp>

#include <voxen/common/pipe_memory_allocator.hpp>

#include "messaging_private.hpp"

namespace voxen::svc
{

using detail::MessageDeleterBlock;
using detail::MessageHeader;
using detail::MessageRequestBlock;
using detail::MessageRouter;

MessageSender::MessageSender(MessageRouter &router, UID my_uid) noexcept : m_router(&router), m_my_uid(my_uid) {}

std::pair<MessageHeader *, void *> MessageSender::allocateStorage(size_t size, bool deleter, bool request)
{
	static_assert(alignof(MessageHeader) <= alignof(void *), "Payload header is over-aligned");
	static_assert(alignof(MessageDeleterBlock) <= alignof(void *), "Deleter block is over-aligned");
	static_assert(alignof(MessageRequestBlock) <= alignof(void *), "Request block is over-aligned");

	static_assert(sizeof(MessageHeader) % alignof(void *) == 0, "Payload start is not aligned with message header");
	static_assert(sizeof(MessageDeleterBlock) % alignof(void *) == 0, "Payload is not aligned with deleter block");
	static_assert(sizeof(MessageRequestBlock) % alignof(void *) == 0, "Payload is not aligned with request block");

	size += sizeof(MessageHeader);

	if (deleter) {
		size += sizeof(MessageDeleterBlock);
	}

	if (request) {
		size += sizeof(MessageRequestBlock);
	}

	void *alloc = PipeMemoryAllocator::allocate(size, alignof(void *));
	MessageHeader *header = new (alloc) MessageHeader(deleter, request);

	return { header, header->payload() };
}

void MessageSender::freeStorage(MessageHeader *header) noexcept
{
	header->releaseRef();
}

void MessageSender::doSend(UID to, UID msg_uid, MessageHeader *header, PayloadDeleter deleter)
{
	header->from_uid = m_my_uid;
	header->msg_uid = msg_uid;

	if (deleter) {
		header->deleterBlock()->deleter = deleter;
	}

	m_router->send(to, header);
}

} // namespace voxen::svc
