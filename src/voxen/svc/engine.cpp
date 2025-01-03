#include <voxen/svc/engine.hpp>

#include <voxen/client/main_thread_service.hpp>
#include <voxen/common/pipe_memory_allocator.hpp>
#include <voxen/debug/uid_registry.hpp>
#include <voxen/server/world.hpp>
#include <voxen/svc/messaging_service.hpp>
#include <voxen/svc/task_service.hpp>
#include <voxen/util/error_condition.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

#include "async_counter_tracker.hpp"

#include <atomic>

namespace voxen::svc
{

namespace
{

auto makeMsgService(ServiceLocator &svc)
{
	return std::make_unique<MessagingService>(svc, MessagingService::Config {});
}

auto makePipeAllocService(ServiceLocator &svc)
{
	return std::make_unique<PipeMemoryAllocator>(svc, PipeMemoryAllocator::Config {});
}

auto makeAsyncCounterTracker(ServiceLocator &)
{
	return std::make_unique<detail::AsyncCounterTracker>();
}

auto makeTaskService(ServiceLocator &svc)
{
	return std::make_unique<TaskService>(svc, TaskService::Config {});
}

auto makeMainThreadService(ServiceLocator &svc)
{
	return std::make_unique<client::MainThreadService>(svc, client::MainThreadService::Config {});
}

auto makeWorldService(ServiceLocator &svc)
{
	return std::make_unique<server::World>(svc);
}

std::atomic_bool g_instance_created = false;

} // namespace

Engine::Engine()
{
	if (g_instance_created.exchange(true) == true) {
		Log::error("Engine instance is already created!");
		throw Exception::fromError(VoxenErrc::AlreadyRegistered, "Engine singleton violated");
	}

	debug::UidRegistry::registerLiteral(MessagingService::SERVICE_UID, "voxen::svc::MessagingService");
	debug::UidRegistry::registerLiteral(PipeMemoryAllocator::SERVICE_UID, "voxen::PipeMemoryAllocator");
	debug::UidRegistry::registerLiteral(detail::AsyncCounterTracker::SERVICE_UID,
		"voxen::svc::detail::AsyncCounterTracker");
	debug::UidRegistry::registerLiteral(TaskService::SERVICE_UID, "voxen::svc::TaskService");
	debug::UidRegistry::registerLiteral(client::MainThreadService::SERVICE_UID, "voxen::client::MainThreadService");
	debug::UidRegistry::registerLiteral(server::World::SERVICE_UID, "voxen::server::World");

	m_service_locator.registerServiceFactory<MessagingService>(makeMsgService);
	m_service_locator.registerServiceFactory<PipeMemoryAllocator>(makePipeAllocService);
	m_service_locator.registerServiceFactory<detail::AsyncCounterTracker>(makeAsyncCounterTracker);
	m_service_locator.registerServiceFactory<TaskService>(makeTaskService);
	m_service_locator.registerServiceFactory<client::MainThreadService>(makeMainThreadService);
	m_service_locator.registerServiceFactory<server::World>(makeWorldService);
}

Engine::~Engine() noexcept
{
	g_instance_created.store(false);
}

std::unique_ptr<Engine> Engine::create()
{
	// Private ctor, `std::make_unique` will not compile
	return std::unique_ptr<Engine>(new Engine);
}

} // namespace voxen::svc
