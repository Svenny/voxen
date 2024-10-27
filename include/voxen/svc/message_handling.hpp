#pragma once

#include <voxen/svc/message_types.hpp>
#include <voxen/visibility.hpp>

namespace voxen::svc
{

namespace detail
{

struct MessageHeader;

}

// Indicates the result of request-class message processing
enum class RequestResult {
	// The request was successfully processed. It only means the request handler function
	// has returned normally, payload might still have a message-specific error state.
	Ok,
	// The request handler function has exited by throwing an exception.
	// You can access and rethrow this exception into your thread.
	Failed,
	// Request was dropped before reaching its destination.
	// Either recipient UID is invalid (it could have went offline)
	// or it has no registered handler for this message type.
	Dropped,
};

// Helper class spawned by the system during message handler invocation.
// Allows to access additional information not stored in the payload.
class VOXEN_API MessageInfo {
public:
	// Implementation-specific constructor.
	// You can't instantiate this object manually.
	explicit MessageInfo(detail::MessageHeader *hdr) : m_hdr(hdr) {}
	MessageInfo(MessageInfo &&) = delete;
	MessageInfo(const MessageInfo &) = delete;
	MessageInfo &operator=(MessageInfo &&) = delete;
	MessageInfo &operator=(const MessageInfo &) = delete;
	~MessageInfo() = default;

	// UID of the agent that sent this message.
	// Keep in mind that it can go offline at any time,
	// even right while you are handling this message.
	UID senderUid() const noexcept;

private:
	detail::MessageHeader *m_hdr = nullptr;
};

// Handler of a "regular", non-empty unicast message.
// Has non-const payload access and can freely modify it.
template<typename F, typename Msg>
concept CMessageHandler = CMessageType<Msg> && std::is_invocable_r_v<void, F, Msg &, MessageInfo &>;

// Handler of an empty unicast message (signal).
// No payload access as its memory is not allocated.
template<typename F, typename Msg>
concept CSignalHandler = CSignalType<Msg> && std::is_invocable_r_v<void, F, MessageInfo &>;

// Handler of a request-class message.
// Has non-const payload access and can freely modify it.
template<typename F, typename Msg>
concept CRequestHandler = CRequestType<Msg> && std::is_invocable_r_v<void, F, Msg &, MessageInfo &>;

// Handler of a non-empty broadcast message.
// Has const payload access and can not modify it.
template<typename F, typename Msg>
concept CBroadcastHandler = CBroadcastType<Msg> && std::is_invocable_r_v<void, F, const Msg &, MessageInfo &>;

// Handler of an empty broadcast message (signal).
// No payload access as its memory is not allocated.
template<typename F, typename Msg>
concept CBroadcastSignalHandler = CBroadcastSignalType<Msg> && std::is_invocable_r_v<void, F, MessageInfo &>;

} // namespace voxen::svc
