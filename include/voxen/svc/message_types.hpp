#pragma once

#include <voxen/common/uid.hpp>

#include <concepts>
#include <type_traits>

namespace voxen::svc
{

// Every message (payload) type belongs to one of these classes
enum class MessageClass {
	// A regular "fire and forget" style message. This is sent
	// to one specified recipient and can't be tracked afterwards.
	// Recipients receive ownership of the message payload.
	Unicast,
	// A request-response style message. This is sent to one specified
	// recipient, who can modify the payload - its ownership is shared
	// until the processing is finished (either successfully or not).
	// Its processing status can be tracked and waited on.
	Request,
	// Broadcast is sent without a recipient specified and can't be tracked.
	// It is received by every agent currently subscribed to this message UID.
	// The payload is shared among all recipients, they can't modify it.
	Broadcast
};

// Base concept covering all message (payload) types:
// - It must define `UID MESSAGE_UID` static constant.
//   UID must be unique among all message types - always generate it randomly and don't reuse.
// - It must define `MessageClass MESSAGE_CLASS` static constant.
//   One type can't belong to several classes. As classes are very functionally
//   different there should be no practical need for this anyway.
// - It must be nothrow destructible (as any sane type should)
// - It must be aligned at most to a pointer. This ensures optimal storage implementation.
//   This restriction might be eventually relaxed to `std::max_align_t`.
template<typename T>
concept CMessageBase = std::is_nothrow_destructible_v<T> && (alignof(T) <= alignof(void *)) && requires {
	{ T::MESSAGE_UID } -> std::same_as<const UID &>;
	{ T::MESSAGE_CLASS } -> std::same_as<const MessageClass &>;
};

// Base concept for all unicast message types
template<typename T>
concept CUnicastMessageBase = CMessageBase<T> && (T::MESSAGE_CLASS == MessageClass::Unicast);

// Base concept for all broadcast message types
template<typename T>
concept CBroadcastMessageBase = CMessageBase<T> && (T::MESSAGE_CLASS == MessageClass::Broadcast);

// Concept for a "regular", non-empty unicast message type
template<typename T>
concept CMessageType = CUnicastMessageBase<T> && !std::is_empty_v<T>;

// Concept for an empty unicast message type called signal.
// Signals have no memory allocated for payloads (think of it as [[no_unique_address]] being
// applied inside the implementation) and consequently their handlers can't access it.
template<typename T>
concept CSignalType = CUnicastMessageBase<T> && std::is_empty_v<T>;

// Concept for a request message type. By its nature it must be non-empty.
template<typename T>
concept CRequestType = CMessageBase<T> && (T::MESSAGE_CLASS == MessageClass::Request) && !std::is_empty_v<T>;

// Concept for a non-empty broadcast message type
template<typename T>
concept CBroadcastType = CBroadcastMessageBase<T> && !std::is_empty_v<T>;

// Concept for an empty broadcast message type called signal.
// Same memory allocation optimization applies as for unicast signals.
template<typename T>
concept CBroadcastSignalType = CBroadcastMessageBase<T> && std::is_empty_v<T>;

} // namespace voxen::svc
