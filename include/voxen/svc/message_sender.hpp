#pragma once

#include <voxen/svc/message_handling.hpp>
#include <voxen/svc/message_types.hpp>
#include <voxen/visibility.hpp>

namespace voxen::svc
{

namespace detail
{

struct MessageHeader;
class MessageRouter;

} // namespace detail

// Provides an interface for one-way messaging (sending only).
// It has a sender UID that might or might not coincide with UID of an existing
// `MessageQueue` instance. If it does coincide with a queue then recipients
// might be able to send their replies to that queue.
//
// Requests with completion messages and broadcasts are not supported deliberately.
// It's not a technical limitation, this restriction can be lifted if there is need.
//
// Object is very lightweight and can be freely copied/moved around.
// However, it must not outlive the instance of `MessagingService` that spawned it.
//
// Unlike `MessageQueue`, this object is thread-safe. It is safe to send from multiple threads
// using the same sender UID, even concurrently with `MessageQueue` having the same UID.
class VOXEN_API MessageSender {
public:
	// Default-initialized message sender is not usable.
	// All functions except ctor/assignment/dtor have undefined behavior.
	MessageSender() = default;
	// Implementation-specific constructor.
	// Use `MessagingService` to instantiate this object.
	explicit MessageSender(detail::MessageRouter &router, UID my_uid) noexcept;
	MessageSender(MessageSender &&other) = default;
	MessageSender(const MessageSender &other) = default;
	MessageSender &operator=(MessageSender &&other) = default;
	MessageSender &operator=(const MessageSender &other) = default;
	~MessageSender() = default;

	// Send a unicast message
	template<CUnicastMessageBase Msg, typename... Args>
	void send(UID to, Args &&...args)
	{
		detail::MessageHeader *header = makeMessageHeader<Msg>(false, std::forward<Args>(args)...);

		if constexpr (std::is_trivially_destructible_v<Msg>) {
			// Don't instantiate empty deleter for trivially destructible payloads
			doSend(to, Msg::MESSAGE_UID, header, nullptr);
		} else {
			doSend(to, Msg::MESSAGE_UID, header, &destroyPayload<Msg>);
		}
	}

	// Send a request message with handle-based tracking.
	// You will receive an `std::future`-like object `RequestHandle`
	// and use it to either periodically check or wait for completion.
	template<CRequestType Msg, typename... Args>
	RequestHandle<Msg> requestWithHandle(UID to, Args &&...args)
	{
		detail::MessageHeader *header = makeMessageHeader<Msg>(true, std::forward<Args>(args)...);
		RequestHandle<Msg> handle(header);

		if constexpr (std::is_trivially_destructible_v<Msg>) {
			// Don't instantiate empty deleter for trivially destructible payloads
			doSend(to, Msg::MESSAGE_UID, header, nullptr);
		} else {
			doSend(to, Msg::MESSAGE_UID, header, &destroyPayload<Msg>);
		}

		return handle;
	}

protected:
	using PayloadDeleter = void (*)(void *) noexcept;

	detail::MessageRouter *m_router = nullptr;
	UID m_my_uid;

	static std::pair<detail::MessageHeader *, void *> allocateStorage(size_t size, bool deleter, bool request);
	static void freeStorage(detail::MessageHeader *header) noexcept;

	void doSend(UID to, UID msg_uid, detail::MessageHeader *header, PayloadDeleter deleter);

	template<CMessageBase Msg, typename... Args>
	static detail::MessageHeader *makeMessageHeader(bool request, Args &&...args)
	{
		if constexpr (std::is_empty_v<Msg>) {
			// Empty messages (signals) need no payload storage
			static_assert(std::is_trivially_destructible_v<Msg>, "Empty type must be trivially destructible");
			return allocateStorage(0, false, request).first;
		} else {
			bool has_deleter = !std::is_trivially_destructible_v<Msg>;
			auto [header, payload] = allocateStorage(sizeof(Msg), has_deleter, request);

			if constexpr (std::is_nothrow_constructible_v<Msg, Args...>) {
				// Don't generate unneeded try-catch code for nothrow constructors
				new (payload) Msg(std::forward<Args>(args)...);
			} else {
				try {
					new (payload) Msg(std::forward<Args>(args)...);
				}
				catch (...) {
					freeStorage(header);
					throw;
				}
			}

			return header;
		}
	}

	template<CMessageBase Msg>
	static void destroyPayload(void *payload) noexcept
	{
		static_cast<Msg *>(payload)->~Msg();
	}
};

} // namespace voxen::svc
