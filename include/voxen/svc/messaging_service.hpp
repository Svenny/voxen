#pragma once

#include <voxen/svc/message_queue.hpp>
#include <voxen/svc/service_base.hpp>

#include <memory>

namespace voxen::svc
{

namespace detail
{

class MessageRouter;

}

class VOXEN_API MessagingService final : public IService {
public:
	constexpr static UID SERVICE_UID = UID("84b390ca-e840e281-b37bf4bf-a99009a7");

	struct Config {};

	MessagingService(ServiceLocator &svc, Config cfg);
	MessagingService(MessagingService &&) = delete;
	MessagingService(const MessagingService &) = delete;
	MessagingService &operator=(MessagingService &&) = delete;
	MessagingService &operator=(const MessagingService &) = delete;
	~MessagingService() noexcept override;

	UID serviceUid() const noexcept override { return SERVICE_UID; }

	// Create a `MessageQueue` object with given UID.
	// Only one queue can be registered for a single UID. Attempting to create
	// another one will throw `Exception` with `VoxenErrc::AlreadyRegistered`.
	//
	// Before this function returns, UID is added to the routing table
	// and messages sent to it begin queuing up in the returned object.
	//
	// NOTE: do process them periodically (see `MessageQueue::pollMessages/waitMessages()`).
	// Failure to do so will effectively leak temporary memory allocations.
	// Even if you don't explicitly expect any incoming messages there can be occasional
	// debugging or internal servicing traffic.
	// So this is not just an optional feature but rather a valid usage requirement.
	// If you don't want to ever deal with incoming messages - create a sender instead.
	MessageQueue registerAgent(UID id);
	// Create a `MessageSender` object with given UID.
	// An unlimited number of senders can be created for the same UID,
	// and their creation does not affect `registerAgent` either.
	//
	// Senders do not participating in message routing. Therefore, any message
	// sent to the sender's UID will be dropped as "destination unreachable"
	// unless there is a `MessageQueue` registered with the same UID.
	//
	// There are three intended use cases for message senders:
	// - Adding the ability for an existing `MessageQueue` to send from "foreign places"
	//   without pulling along the reference to it. Note that you can directly construct
	//   a sender from a queue, calling this function is not necessary in this case.
	// - Creating an agent that will only emit messages, not expecting any reply.
	//   This way you don't risk leaking memory because of unprocessed incoming messages.
	// - For debugging purposes, to "impersonate" any other agent. Note that doing
	//   so in production is strongly discouraged, this masquerade makes
	//   understanding communication flow of the system extremely hard.
	MessageSender createSender(UID id);

private:
	std::unique_ptr<detail::MessageRouter> m_router;
};

} // namespace voxen::svc
