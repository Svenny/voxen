#include <voxen/svc/messaging_service.hpp>

#include <voxen/common/pipe_memory_allocator.hpp>
#include <voxen/debug/debug_uid_registry.hpp>
#include <voxen/svc/service_locator.hpp>

#include "messaging_private.hpp"

namespace voxen::svc
{

MessagingService::MessagingService(ServiceLocator &svc, Config /*cfg*/)
{
	debug::UidRegistry::registerLiteral(SERVICE_UID, "voxen/service/MessagingService");

	svc.requestService<PipeMemoryAllocator>();
	m_router = std::make_unique<detail::MessageRouter>();
}

MessagingService::~MessagingService() noexcept = default;

MessageQueue MessagingService::registerAgent(UID id)
{
	return MessageQueue(*m_router, id);
}

} // namespace voxen::svc
