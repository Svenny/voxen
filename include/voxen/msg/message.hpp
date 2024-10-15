#pragma once

#include <voxen/common/uid.hpp>
#include <voxen/msg/message_payload.hpp>
#include <voxen/visibility.hpp>

#include <system_error>

namespace voxen::msg
{

class VOXEN_API MessageBase {
public:
	UID from() const noexcept { return m_from; }
	uint32_t tag() const noexcept { return m_tag; }
	bool isBroadcast() const noexcept;
	bool isRequest() const noexcept;

	void reply();
	void setReplyStatus(bool status);
	void setReplyCode(std::error_condition code);

protected:
	UID m_from;
	uint32_t m_tag = 0;
	uint32_t m_flags = 0;

	void *getPayloadPtr() const noexcept;
};

template<MessagePayload P>
class VOXEN_API Message : public MessageBase {
public:
	P &payload() noexcept { return reinterpret_cast<P &>(getPayloadPtr()); }
	const P &payload() const noexcept { return reinterpret_cast<const P &>(getPayloadPtr()); }

	static Message &from(MessageBase &msg) noexcept
	{
		VOXEN_HOT_ASSERT(msg.tag() == P::MESSAGE_TAG, "Stored payload tag doesn't match the requested");
		return static_cast<Message &>(msg);
	}

	static const Message &from(const MessageBase &msg) noexcept
	{
		VOXEN_HOT_ASSERT(msg.tag() == P::MESSAGE_TAG, "Stored payload tag doesn't match the requested");
		return static_cast<const Message &>(msg);
	}
};

} // namespace voxen::msg
