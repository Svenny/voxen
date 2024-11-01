#pragma once

#include <voxen/svc/message_handling.hpp>
#include <voxen/svc/message_types.hpp>
#include <voxen/visibility.hpp>

#include <extras/move_only_function.hpp>
#include <extras/pimpl.hpp>

namespace voxen::svc
{

namespace detail
{

struct MessageHeader;
class MessageRouter;

} // namespace detail

// NOTE: this object is NOT thread-safe at all. In fact, using one
// message queue from several threads is strongly discouraged.
class VOXEN_API MessageQueue {
public:
	// Default-initialized message queue is not usable.
	// All functions except ctor/assignment/dtor have undefined behavior.
	MessageQueue() noexcept;
	// Implementation-specific constructor.
	// Use `MessagingService` to instantiate this object.
	MessageQueue(detail::MessageRouter &router, UID my_uid);
	MessageQueue(MessageQueue &&) noexcept;
	MessageQueue(const MessageQueue &) = delete;
	MessageQueue &operator=(MessageQueue &&) noexcept;
	MessageQueue &operator=(const MessageQueue &) = delete;
	// All unprocessed incoming messages are dropped by destructor
	~MessageQueue() noexcept;

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

	// Send a request message with completion message-based tracking.
	// Once the message processing has finished in any way (i.e. changes `RequestStatus`
	// to any value other than `Pending`) it will be forwarded back to this queue.
	// You will then receive it together with regular incoming messages
	// and can register a handler function with `registerCompletionHandler`.
	template<CRequestType Msg, typename... Args>
	void requestWithCompletion(UID to, Args &&...args)
	{
		detail::MessageHeader *header = makeMessageHeader<Msg>(true, std::forward<Args>(args)...);

		if constexpr (std::is_trivially_destructible_v<Msg>) {
			// Don't instantiate empty deleter for trivially destructible payloads
			doRequestWithCompletion(to, Msg::MESSAGE_UID, header, nullptr);
		} else {
			doRequestWithCompletion(to, Msg::MESSAGE_UID, header, &destroyPayload<Msg>);
		}
	}

	// TODO: send a broadcast message
	// template<CBroadcastMessageBase Msg, typename... Args>
	// void broadcast(Args &&...args);

	// Register handler function for a non-empty unicast message
	template<CMessageType Msg, CMessageHandler<Msg> F>
	void registerHandler(F &&fn)
	{
		doRegisterHandler(Msg::MESSAGE_UID, MessageHandler { [f = std::forward<F>(fn)](MessageInfo &info, void *payload) {
			f(*static_cast<Msg *>(payload), info);
		} });
	}

	// Register handler function for an empty unicast message (signal)
	template<CSignalType Msg, CSignalHandler<Msg> F>
	void registerHandler(F &&fn)
	{
		doRegisterHandler(Msg::MESSAGE_UID,
			MessageHandler { [f = std::forward<F>(fn)](MessageInfo &info, void *) { f(info); } });
	}

	// Register handler function for a request message
	template<CRequestType Msg, CRequestHandler<Msg> F>
	void registerHandler(F &&fn)
	{
		doRegisterHandler(Msg::MESSAGE_UID, MessageHandler { [f = std::forward<F>(fn)](MessageInfo &info, void *payload) {
			f(*static_cast<Msg *>(payload), info);
		} });
	}

	// Register handler function for a non-empty broadcast message
	template<CBroadcastType Msg, CBroadcastHandler<Msg> F>
	void registerHandler(F &&fn)
	{
		doRegisterHandler(Msg::MESSAGE_UID, MessageHandler { [f = std::forward<F>(fn)](MessageInfo &info, void *payload) {
			f(*static_cast<const Msg *>(payload), info);
		} });
	}

	// Register handler function for an empty broadcast message (signal)
	template<CBroadcastSignalType Msg, CBroadcastSignalHandler<Msg> F>
	void registerHandler(F &&fn)
	{
		doRegisterHandler(Msg::MESSAGE_UID,
			MessageHandler { [f = std::forward<F>(fn)](MessageInfo &info, void *) { f(info); } });
	}

	template<CRequestType Msg, CRequestCompletionHandler<Msg> F>
	void registerCompletionHandler(F &&fn)
	{
		doRegisterCompletionHandler(Msg::MESSAGE_UID,
			CompletionHandler { [f = std::forward<F>(fn)](RequestCompletionInfo &info, void *payload) {
				f(*static_cast<Msg *>(payload), info);
			} });
	}

	// Remove a registered handler. Any further incoming message
	// of this type, including those currently queued, will be dropped.
	template<CMessageBase Msg>
	void unregisterHandler() noexcept
	{
		doUnregisterHandler(Msg::MESSAGE_UID);
	}

	// Remove a registered completion handler. Any further completion message
	// for this request, including those currently queued, will be dropped.
	template<CRequestType Msg>
	void unregisterCompletionHandler() noexcept
	{
		doUnregisterCompletionHandler(Msg::MESSAGE_UID);
	}

	// Receive all incoming messages queued at this moment and call their
	// corresponding handler functions (registered earlier).
	// Returns immediately if no messages are queued.
	//
	// NOTE: you must receive messages periodically even if you have no intent to process them.
	// Otherwise they will remain queued indefinitely, effectively leaking allocated memory.
	//
	// NOTE: behavior is undefined if this function is called from a message handler.
	void pollMessages();

	// Block until any message comes in, then do the same as `pollMessages()`.
	// No blocking will occur if there are queued messages.
	// Waits for at most `timeout_msec` milliseconds.
	//
	// NOTE: exercise caution using this function if there are several message
	// queues owned by the same thread. You can easily deadlock with it.
	//
	// NOTE: behavior is undefined if this function is called from a message handler.
	void waitMessages(uint32_t timeout_msec = UINT32_MAX);

private:
	using PayloadDeleter = void (*)(void *) noexcept;
	using MessageHandler = extras::move_only_function<void(MessageInfo &, void *)>;
	using CompletionHandler = extras::move_only_function<void(RequestCompletionInfo &, void *)>;
	using HandlerItem = std::pair<UID, MessageHandler>;
	using CompletionHandlerItem = std::pair<UID, CompletionHandler>;

	struct Impl;
	extras::pimpl<Impl, 96, 8> m_impl;

	static std::pair<detail::MessageHeader *, void *> allocateStorage(size_t size, bool deleter, bool request);
	static void freeStorage(detail::MessageHeader *header) noexcept;
	static bool handlerComparator(const HandlerItem &a, const HandlerItem &b) noexcept;
	static bool completionHandlerComparator(const CompletionHandlerItem &a, const CompletionHandlerItem &b) noexcept;

	void doSend(UID to, UID msg_uid, detail::MessageHeader *header, PayloadDeleter deleter);
	void doRequestWithCompletion(UID to, UID msg_uid, detail::MessageHeader *header, PayloadDeleter deleter);

	void doRegisterHandler(UID msg_uid, MessageHandler handler);
	void doRegisterCompletionHandler(UID msg_uid, CompletionHandler handler);
	void doUnregisterHandler(UID msg_uid) noexcept;
	void doUnregisterCompletionHandler(UID msg_uid) noexcept;

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
