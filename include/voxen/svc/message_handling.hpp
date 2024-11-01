#pragma once

#include <voxen/svc/message_types.hpp>
#include <voxen/visibility.hpp>

namespace voxen::svc
{

namespace detail
{

struct MessageHeader;

}

// Indicates the status of request-class message processing
enum class RequestStatus : uint32_t {
	// The request is awaiting processing - its handler function has not been called yet.
	Pending = 0,
	// The request was successfully processed. It only means the request handler function
	// has returned normally, payload might still have a message-specific error state.
	Complete = 1,
	// The request handler function has exited by throwing an exception.
	// You can access and rethrow this exception into your thread.
	Failed = 2,
	// Request was dropped before reaching its destination.
	// Either recipient UID is invalid (it could have went offline)
	// or it has no registered handler for this message type.
	Dropped = 3,
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

protected:
	detail::MessageHeader *m_hdr = nullptr;
};

class VOXEN_API RequestCompletionInfo : public MessageInfo {
public:
	using MessageInfo::MessageInfo;

	RequestStatus status() const noexcept;
	void rethrowIfFailed();
};

class VOXEN_API RequestHandleBase {
public:
	RequestHandleBase() = default;
	explicit RequestHandleBase(detail::MessageHeader *hdr) noexcept;
	RequestHandleBase(RequestHandleBase &&other) noexcept;
	RequestHandleBase(const RequestHandleBase &) = delete;
	RequestHandleBase &operator=(RequestHandleBase &&other) noexcept;
	RequestHandleBase &operator=(const RequestHandleBase &) = delete;
	~RequestHandleBase() noexcept;

	bool valid() const noexcept { return m_hdr != nullptr; }
	void *payload() const noexcept;

	RequestStatus wait() noexcept;
	RequestStatus status() noexcept;
	void rethrowIfFailed();

protected:
	detail::MessageHeader *m_hdr = nullptr;
};

template<CRequestType Msg>
class VOXEN_API RequestHandle : public RequestHandleBase {
public:
	using RequestHandleBase::RequestHandleBase;

	Msg &payload() { return *static_cast<Msg *>(RequestHandleBase::payload()); }
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

// Completion handler of a request-class message.
// Has non-const payload access and can freely modify it.
template<typename F, typename Msg>
concept CRequestCompletionHandler = CRequestType<Msg> && std::is_invocable_r_v<void, F, Msg &, RequestCompletionInfo &>;

// Handler of a non-empty broadcast message.
// Has const payload access and can not modify it.
template<typename F, typename Msg>
concept CBroadcastHandler = CBroadcastType<Msg> && std::is_invocable_r_v<void, F, const Msg &, MessageInfo &>;

// Handler of an empty broadcast message (signal).
// No payload access as its memory is not allocated.
template<typename F, typename Msg>
concept CBroadcastSignalHandler = CBroadcastSignalType<Msg> && std::is_invocable_r_v<void, F, MessageInfo &>;

} // namespace voxen::svc
