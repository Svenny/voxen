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

// Extension of `MessageInfo` for request completion handler invocations
class VOXEN_API RequestCompletionInfo : public MessageInfo {
public:
	using MessageInfo::MessageInfo;

	// Result of message processing, can't be `Pending`
	RequestStatus status() const noexcept;
	// If `status() == Failed` the stored exception will be rethrown in the current thread
	void rethrowIfFailed();
};

// Base class for `RequestHandle<T>`, see its description
class VOXEN_API RequestHandleBase {
public:
	// Default constructor creates an invalid (unusable) handle
	RequestHandleBase() = default;
	// Implementation-specific constructor, use `MessageQueue::request()`
	explicit RequestHandleBase(detail::MessageHeader *hdr) noexcept;
	RequestHandleBase(RequestHandleBase &&other) noexcept;
	RequestHandleBase(const RequestHandleBase &) = delete;
	RequestHandleBase &operator=(RequestHandleBase &&other) noexcept;
	RequestHandleBase &operator=(const RequestHandleBase &) = delete;
	~RequestHandleBase() noexcept;

	// True if this handle points to a valid request message
	bool valid() const noexcept { return m_hdr != nullptr; }

	// Drop message reference, `valid()` becomes false after this call
	void reset() noexcept;

	// Raw payload address, use `RequestHandle<T>` for convenience.
	// Behavior is undefined if `valid() == false`.
	void *payload() noexcept;

	// Block until message processing completes, i.e. `status()` becomes not equal to `Pending`.
	// Behavior is undefined if `valid() == false`.
	RequestStatus wait() noexcept;
	// Asynchronously check the current message processing status.
	// Initially it is `Pending`, and when it changes to any othe value,
	// that value will remain unchanged as long as this handle is valid.
	// Behavior is undefined if `valid() == false`.
	RequestStatus status() noexcept;
	// If `status() == Failed` the stored exception will be rethrown in the current thread.
	// Behavior is undefined if `status() == Pending` or `valid() == false`.
	void rethrowIfFailed();

protected:
	detail::MessageHeader *m_hdr = nullptr;
};

// Provides payload access and status tracking for a sent request message.
//
// NOTE: message payload and some control data remains allocated while
// this handle is valid. Don't store it for indefinite periods, eliminate
// ownership as soon as you can by destroying the handle or calling `reset()`.
template<CRequestType Msg>
class VOXEN_API RequestHandle : public RequestHandleBase {
public:
	using RequestHandleBase::RequestHandleBase;

	// Read-write access to payload is always available while this handle is valid.
	// NOTE: payload can be concurrently accessed by the recipient until request
	// processing has finished, i.e. while `status() == Pending`.
	Msg &payload() noexcept { return *static_cast<Msg *>(RequestHandleBase::payload()); }
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
