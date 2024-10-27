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

class VOXEN_API MessageQueue {
public:
	MessageQueue(detail::MessageRouter &router, UID my_uid);
	MessageQueue(MessageQueue &&) = delete;
	MessageQueue(const MessageQueue &) = delete;
	MessageQueue &operator=(MessageQueue &&) = delete;
	MessageQueue &operator=(const MessageQueue &) = delete;
	~MessageQueue() noexcept;

	// Send a unicast message
	template<CUnicastMessageBase Msg, typename... Args>
	void send(UID to, Args &&...args)
	{
		void *alloc = nullptr;

		// Empty messages (signals) need no payload storage
		if constexpr (std::is_empty_v<Msg>) {
			alloc = allocatePayloadStorage(0).first;
		} else {
			void *payload = nullptr;
			std::tie(alloc, payload) = allocatePayloadStorage(sizeof(Msg));

			if constexpr (std::is_nothrow_constructible_v<Msg, Args...>) {
				// Don't generate unneeded try-catch code for nothrow constructors
				new (payload) Msg(std::forward<Args>(args)...);
			} else {
				try {
					new (payload) Msg(std::forward<Args>(args)...);
				}
				catch (...) {
					freePayloadStorage(alloc);
					throw;
				}
			}
		}

		if constexpr (std::is_trivially_destructible_v<Msg>) {
			// Don't instantiate empty deleter for trivially destructible payloads
			doSend(to, Msg::MESSAGE_UID, alloc, nullptr);
		} else {
			doSend(to, Msg::MESSAGE_UID, alloc, &destroyPayload<Msg>);
		}
	}

	// TODO: send a request message
	// template<CRequestType Msg, typename... Args>
	// <TODO> request(UID to, Args &&...args);

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

	// Remove a registered handler. Any further incoming message
	// of this type, including those currently queued, will be dropped.
	template<CMessageBase Msg>
	void unregisterHandler() noexcept
	{
		doUnregisterHandler(Msg::MESSAGE_UID);
	}

	// Receive all incomming messages queued at this moment
	// and call their corresponding handler functions
	void receiveAll();

private:
	using PayloadDeleter = void (*)(void *) noexcept;
	using MessageHandler = extras::move_only_function<void(MessageInfo &, void *)>;
	using HandlerItem = std::pair<UID, MessageHandler>;

	struct Impl;
	extras::pimpl<Impl, 64, 8> m_impl;

	static std::pair<void *, void *> allocatePayloadStorage(size_t size);
	static void freePayloadStorage(void *alloc) noexcept;
	static bool handlerComparator(const HandlerItem &a, const HandlerItem &b) noexcept;

	void doSend(UID to, UID msg_uid, void *alloc, PayloadDeleter deleter);
	void doRegisterHandler(UID msg_uid, MessageHandler handler);
	void doUnregisterHandler(UID msg_uid) noexcept;

	template<CMessageBase Msg>
	static void destroyPayload(void *payload) noexcept
	{
		static_cast<Msg *>(payload)->~Msg();
	}
};

} // namespace voxen::svc
