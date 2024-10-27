#include <voxen/svc/service_locator.hpp>

#include <voxen/debug/debug_uid_registry.hpp>
#include <voxen/util/error_condition.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

#include <extras/defer.hpp>

#include <algorithm>
#include <cassert>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace voxen::svc
{

struct ServiceLocator::Impl {
	// Protects access to all fields. Locked shared for read-only operations
	// and exclusive for read-write ones (to register/start services).
	std::shared_mutex lock;

	// Active (started) services, stopped only at exit
	std::unordered_map<UID, std::unique_ptr<IService>> active_services;

	// Registered factory functions for services.
	// We will fail creation if a new service is requested but there is no factory for it.
	std::unordered_map<UID, ServiceFactoryFunction> factory_map;

	// Stores services' UIDs in order of their start.
	// Stopping (destruction) will happen in reverse order.
	std::vector<UID> start_order;

	// Pending (starting but not yet started) services.
	// Essentially a call stack of `requestService`.
	std::vector<UID> pending_services;

	// See `requestService` code. Flag is set when a service start failure
	// occurred to avoid duplicate logging during stack unwinding.
	bool request_service_fail_logged = false;

	// Print the dependency chain of pending services that can't be started.
	// `lock` must be held (at least shared) while entering this function.
	void errorLogDependencyChain() const
	{
		if (pending_services.size() == 1) {
			// Chain of one service - no other pending services fail to start because of it
			return;
		}

		Log::error("Failed dependency chain:");

		for (auto iter = pending_services.rbegin(); iter != pending_services.rend(); ++iter) {
			size_t index = static_cast<size_t>(std::distance(pending_services.rbegin(), iter));
			std::string str = debug::UidRegistry::lookup(*iter);

			if (index == 0) {
				Log::error("    {}, dependency chain end (failure point)", str);
			} else {
				Log::error("    {}, dependency level {}", str, pending_services.size() - index);
			}
		}
	}
};

ServiceLocator::ServiceLocator() = default;

ServiceLocator::~ServiceLocator() noexcept
{
	// Take exclusive lock, we're modifying the structure.
	// Locking is theoretically a potentially throwing operation... but if it does eventually
	// throw then we're so screwed that exception in dtor is the lesser of our problems.
	std::unique_lock lock(m_impl->lock);

	Log::debug("Destroying services");
	for (auto iter = m_impl->start_order.rbegin(); iter != m_impl->start_order.rend(); ++iter) {
		Log::debug("Destroying service {}", debug::UidRegistry::lookup(*iter));

		// Remove this service from the set (make it non-accessible)
		auto service_iter = m_impl->active_services.find(*iter);
		assert(service_iter != m_impl->active_services.end());
		std::unique_ptr<IService> service = std::move(service_iter->second);
		m_impl->active_services.erase(service_iter);

		// Destroy it temporarily dropping the lock so that
		// the service destructor is free to call `findService`
		lock.unlock();
		service.reset();
		lock.lock();
	}
	Log::debug("All services destroyed");
}

IService *ServiceLocator::findService(UID id)
{
	std::shared_lock lk(m_impl->lock);
	auto iter = m_impl->active_services.find(id);
	return iter != m_impl->active_services.end() ? iter->second.get() : nullptr;
}

IService &ServiceLocator::requestService(UID id)
{
	// Check if service is already started first (with shared lock)
	if (IService *service = findService(id); service != nullptr) {
		return *service;
	}

	// Okay, this service is not started, take exclusive lock.
	// We must be VERY careful to temporarily release it while calling factory
	// functions, their service dependencies will make recursive calls here.
	std::unique_lock lock(m_impl->lock);

	// Reset "fail logged" flag - there can't be failures while we're still entering this section
	m_impl->request_service_fail_logged = false;

	// Check for circular dependency.
	// We have a set of pending services. They are starting now in a recursive call chain.
	// A circular dependency occurs if and only if a single ID is added to this set twice.
	// This means a service requires itself to be started before it can start.
	//
	// - Current ID is pending (or we'd have returned from the check above)
	// - We are going to add current ID to `pending_services` right now
	// - Therefore, if it's already there we have a circular dependency
	// - Otherwise we don't have it (at least so far)
	//
	// Linear search over the vector is totally fine, pending set should contain just a few items.
	if (std::ranges::find(m_impl->pending_services, id) != m_impl->pending_services.end()) {
		// This can happen only once per call chain, no need to check this flag
		m_impl->request_service_fail_logged = true;

		Log::error("Circular service dependency! Service {} needs itself to start", debug::UidRegistry::lookup(id));
		m_impl->errorLogDependencyChain();
		throw Exception::fromError(VoxenErrc::CircularDependency, "circular service dependency");
	}

	// Add ID to the pending stack before calling its factory.
	// Otherwise we'd get infinite recursion instead of circular dependency detection.
	m_impl->pending_services.emplace_back(id);

	// Roll back insertion after leaving this function (pop from stack)
	defer {
		assert(m_impl->pending_services.back() == id);
		m_impl->pending_services.pop_back();
	};

	// Find the factory function
	auto factory_iter = m_impl->factory_map.find(id);
	if (factory_iter == m_impl->factory_map.end()) {
		// This can happen only once per dependency chain, no need to check this flag
		m_impl->request_service_fail_logged = true;

		Log::error("No factory registered for requested service {}!", debug::UidRegistry::lookup(id));
		m_impl->errorLogDependencyChain();
		throw Exception::fromError(VoxenErrc::UnresolvedDependency, "requested non-registered service");
	}

	std::unique_ptr<IService> service;

	// Construct the service - here it can call `requestService` recursively.
	// Pass factory exceptions through but log failures.
	try {
		// Release the lock during factory call or we will deadlock.
		// Recursive mutex wouldn't help here - it's OK (though quite weird)
		// for a factory to start another thread and request services from it.
		lock.unlock();
		service = factory_iter->second(*this);
		lock.lock();
	}
	catch (...) {
		lock.lock();

		// We can either catch this exception from the bottom of dependency chain
		// (factory function failure) or transitively from another `requestService`
		if (!m_impl->request_service_fail_logged) {
			// Bottom of dependency chain (original failure)
			m_impl->request_service_fail_logged = true;

			Log::error("Factory of service {} failed!", debug::UidRegistry::lookup(id));
			m_impl->errorLogDependencyChain();
		} else {
			// Transitive failure
			Log::error("Can't start service {} because starting its dependency failed!", debug::UidRegistry::lookup(id));
		}

		throw;
	}

	// Service pointer must be non-zero - that's factory interface contract
	assert(service);
	IService *constructed_service = service.get();

	m_impl->active_services.emplace(id, std::move(service));
	m_impl->start_order.emplace_back(id);
	Log::debug("Started service {}", debug::UidRegistry::lookup(id));

	return *constructed_service;
}

void ServiceLocator::registerServiceFactory(UID id, ServiceFactoryFunction factory)
{
	// Take exclusive lock, we're modifying the structure
	std::lock_guard lk(m_impl->lock);

	if (!m_impl->factory_map.emplace(id, std::move(factory)).second) {
		Log::error("Factory is already registered for service UID {}!", debug::UidRegistry::lookup(id));
		throw Exception::fromError(VoxenErrc::AlreadyRegistered, "double-register service factory");
	}
}

} // namespace voxen::svc
