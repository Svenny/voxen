#include <voxen/svc/engine.hpp>

#include <voxen/common/pipe_memory_allocator.hpp>
#include <voxen/common/thread_pool.hpp>
#include <voxen/svc/messaging_service.hpp>

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

auto makeThreadPoolService(ServiceLocator &svc)
{
	return std::make_unique<ThreadPool>(svc, ThreadPool::Config {});
}

} // namespace

Engine::Engine()
{
	m_service_locator.registerServiceFactory<MessagingService>(makeMsgService);
	m_service_locator.registerServiceFactory<PipeMemoryAllocator>(makePipeAllocService);
	m_service_locator.registerServiceFactory<ThreadPool>(makeThreadPoolService);
}

std::unique_ptr<Engine> Engine::create()
{
	return std::unique_ptr<Engine>(new Engine);
}

} // namespace voxen::svc
