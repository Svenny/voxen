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

	MessageQueue registerAgent(UID id);

private:
	std::unique_ptr<detail::MessageRouter> m_router;
};

} // namespace voxen::svc
