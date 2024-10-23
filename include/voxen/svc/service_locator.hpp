#pragma once

#include <voxen/svc/service_base.hpp>

#include <extras/pimpl.hpp>

#include <functional>
#include <memory>

namespace voxen::svc
{

// This class controls service startup, dependencies, search and shutdown.
// It is not a singleton per se, but there should be no need to ever create more than
// one instance per program. It needs no special startup/shutdown routine.
//
// See `IService` description (or better, existing implementations) to understand what a service is.
// How to use it:
// - Implement a service class (see `IService` and `CService`)
// - Implement a factory function that will create its instance (see `TServiceFactoryFunction`)
// - Register class(UID)=>factory mapping with `registerServiceFactory()`
// - Call `requestService()` when this service is needed. It can be requested
//   either directly or from a factory of another service (as dependency).
// - Once started, a service becomes accessible via `findService()`
//   and will remain active for the whole lifetime of `ServiceLocator`
// - When this class is destroyed, active services are destroyed in reverse of their start order
//
// Class functions are fully thread-safe.
// Class functions guarantee strong exception safety with OOM exception.
class VOXEN_API ServiceLocator {
public:
	// Typed factory function for a service; internally cast to a generic one via pointer covariance.
	// It must either return a non-null pointer to a fully constructed, operational service,
	// or throw an exception if that is not possible.
	//
	// Factory will be called as a callback from `requestService` - either a direct request
	// or as a transitive dependency of another service. Factory is executed on a thread
	// that initiated the first call of a dependency tree. In other words, don't expect
	// the current thread ID to have any particular value (main thread or any other).
	//
	// Any method of provided `ServiceLocator` reference can be called safely.
	// Factory can (and should) request dependencies of the service being constructed.
	// However, circular dependencies are forbidden and will be detected.
	//
	// `ServiceLocator` reference can be safely saved inside the created service.
	// Locator's lifetime is guaranteed to exceed that of any service it owns.
	template<CService Service>
	using TServiceFactoryFunction = std::function<std::unique_ptr<Service>(ServiceLocator &)>;
	// Generic factory function for a service; for internal usage
	using ServiceFactoryFunction = std::function<std::unique_ptr<IService>(ServiceLocator &)>;

	ServiceLocator();
	ServiceLocator(ServiceLocator &&) = delete;
	ServiceLocator(const ServiceLocator &) = delete;
	ServiceLocator &operator=(ServiceLocator &&) = delete;
	ServiceLocator &operator=(const ServiceLocator &) = delete;

	// All active services are destroyed in the reverse order of their start.
	// Therefore any service can use its dependencies correctly while destroying.
	//
	// Service destructors are called sequentially in the same thread as this destructor.
	// Registered factory functions are destroyed sequentially in the same thread,
	// but in an unspecified order relative to each other and their corresponding services.
	//
	// NOTE: a service destructor might be executed on a thread different from
	// where its factory was executed. In other words, don't expect the current
	// thread ID to have any particular value.
	//
	// A service destructor is free to call `findService`. Note that the service
	// being destructed and those started after it will not be accessible anymore.
	// NOTE: however, calling `requestService` there will cause undefined behavior.
	~ServiceLocator() noexcept;

	// If a service with this ID is started, returns a valid pointer to it.
	// Otherwise returns null pointer. Returned pointer is valid for the whole
	// lifetime of `ServiceLocator`.
	// This function throws no exceptions unless a system locks error occurs.
	IService *findService(UID id);

	// If a service with this ID is already started, behaves as `*findService(id)`.
	//
	// Otherwise it performs these steps:
	// - If no factory function for this service is registered,
	//   throws `Exception` with `VoxenErrc::UnresolvedDependency`
	// - Calls the registered factory function. If it throws an exception,
	//   it is passed through, no state is changed - strong exception safety.
	// - The factory function might do recursive calls to `requestService` to start
	//   dependencies before the service it constructs. If a circular dependency
	//   is detected here, locator throws `Exception` with `VoxenErrc::CircularDependency`.
	// - Factory function returns a valid service pointer which is made
	//   available to further `findService/requestService` calls.
	//
	// Returns reference to the created service.
	// Factories for the whole dependency tree will be executed on the current thread.
	//
	// NOTE: it is undefined behavior to call this function from a service destructor.
	IService &requestService(UID id);

	// Typed helper for `findService`
	template<CService Service>
	Service *findService()
	{
		return static_cast<Service *>(findService(Service::SERVICE_UID));
	}

	// Typed helper for `requestService`
	template<CService Service>
	IService &requestService()
	{
		return static_cast<Service &>(requestService(Service::SERVICE_UID));
	}

	// Register a factory function for a service, see `TServiceFactoryFunction`.
	// It will be called later to start this service, if requested.
	//
	// Registration is permanent, a service cannot be unregistered
	// during the lifetime of this `ServiceLocator` instance.
	//
	// Double registration of the same service (by UID) is not allowed, this function
	// will throw `Exception` with `VoxenErrc::AlreadyRegistered` in this case.
	template<CService Service>
	void registerServiceFactory(TServiceFactoryFunction<Service> factory)
	{
		registerServiceFactory(Service::SERVICE_UID, std::move(factory));
	}

private:
	struct Impl;
	extras::pimpl<Impl, 288, 8> m_impl;

	void registerServiceFactory(UID id, ServiceFactoryFunction factory);
};

} // namespace voxen::svc
