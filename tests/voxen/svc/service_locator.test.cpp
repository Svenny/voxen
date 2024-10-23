#include <voxen/svc/service_locator.hpp>

#include "../../voxen_test_common.hpp"

#include <fmt/format.h>

#include <thread>

namespace voxen::svc
{

namespace
{

// Trivial service dependent on a list of other services
template<UID U, CService... Deps>
class DepService : public IService {
public:
	constexpr static UID SERVICE_UID = U;

	DepService(ServiceLocator& svc) noexcept : m_svc(svc) {}

	~DepService() override
	{
		// Ensure dependencies are still active.
		// Stresses both destruction order and the ability
		// to call `findService` from service destructors.
		if constexpr (sizeof...(Deps) > 0) {
			constexpr UID uids[] = { Deps::SERVICE_UID... };

			for (UID u : uids) {
				INFO(fmt::format("Service {} dtor checks if dependency UID {} is still active", U, u));
				CHECK(m_svc.findService(u) != nullptr);
			}
		}
	}

	UID serviceUid() const noexcept override { return SERVICE_UID; }

	static std::unique_ptr<DepService> factory(ServiceLocator& svc)
	{
		requestDeps(svc);
		return std::make_unique<DepService>(svc);
	}

protected:
	ServiceLocator& m_svc;

	static void requestDeps(ServiceLocator& svc)
	{
		if constexpr (sizeof...(Deps) > 0) {
			constexpr UID uids[] = { Deps::SERVICE_UID... };

			for (UID u : uids) {
				svc.requestService(u);
			}
		}
	}
};

// `DepService` that fails creation after requesting its dependencies
template<UID U, CService... Deps>
class BadService : public DepService<U, Deps...> {
public:
	static std::unique_ptr<BadService> factory(ServiceLocator& svc)
	{
		DepService<U, Deps...>::requestDeps(svc);
		throw Exception::fromError(std::errc::not_supported, "oops");
	}
};

// `DepService` that registers factories for its dependencies from its own factory
template<UID U, CService... Deps>
class RecursiveService : public DepService<U, Deps...> {
public:
	RecursiveService(ServiceLocator& svc) noexcept : DepService<U, Deps...>(svc) {}

	template<CService S>
	static void registerDependency(ServiceLocator& svc)
	{
		CHECK_NOTHROW(svc.registerServiceFactory<S>(S::factory));
		CHECK_NOTHROW(svc.requestService<S>());
	}

	static std::unique_ptr<RecursiveService> factory(ServiceLocator& svc)
	{
		(..., registerDependency<Deps>(svc));
		return std::make_unique<RecursiveService>(svc);
	}
};

// `DepService` that requests is dependencies from a secondary thread
template<UID U, CService... Deps>
class ThreadedService : public DepService<U, Deps...> {
public:
	ThreadedService(ServiceLocator& svc) noexcept : DepService<U, Deps...>(svc) {}

	static void threadFunc(ServiceLocator& svc) { DepService<U, Deps...>::requestDeps(svc); }

	static std::unique_ptr<ThreadedService> factory(ServiceLocator& svc)
	{
		std::thread t(threadFunc, std::ref(svc));
		t.join();
		return std::make_unique<ThreadedService>(svc);
	}
};

} // namespace

TEST_CASE("'ServiceLocator' sanity check", "[voxen::svc::service_locator]")
{
	using ServiceA = DepService<UID("1fc82db5-ea75f28a-c21c223b-10663645")>;
	using ServiceB = DepService<UID("c2b6fae1-a1aded58-0f054134-53d47bec"), ServiceA>;
	using ServiceC = DepService<UID("dc098141-b47700f8-2d43b146-c5c74611"), ServiceA>;
	using ServiceD = DepService<UID("8819c518-0260c91d-db31ab20-f0daee10"), ServiceB, ServiceC>;
	using ServiceE = DepService<UID("eb934a1d-ea3777fe-8aeaf67f-13149325"), ServiceB, ServiceD>;

	ServiceLocator svc;
	CHECK(svc.findService<ServiceA>() == nullptr);

	CHECK_NOTHROW(svc.registerServiceFactory<ServiceA>(ServiceA::factory));
	CHECK(svc.findService<ServiceA>() == nullptr);

	CHECK_NOTHROW(svc.registerServiceFactory<ServiceB>(ServiceB::factory));
	CHECK(svc.findService<ServiceA>() == nullptr);
	CHECK(svc.findService<ServiceB>() == nullptr);

	CHECK_NOTHROW(svc.registerServiceFactory<ServiceC>(ServiceC::factory));
	CHECK_NOTHROW(svc.registerServiceFactory<ServiceD>(ServiceD::factory));
	CHECK_NOTHROW(svc.registerServiceFactory<ServiceE>(ServiceE::factory));

	CHECK_NOTHROW(svc.requestService<ServiceA>());
	CHECK(svc.findService<ServiceA>() != nullptr);
	CHECK(svc.findService<ServiceB>() == nullptr);

	CHECK_NOTHROW(svc.requestService<ServiceB>());
	CHECK(svc.findService<ServiceA>() != nullptr);
	CHECK(svc.findService<ServiceB>() != nullptr);
	CHECK(svc.findService<ServiceC>() == nullptr);
	CHECK(svc.findService<ServiceD>() == nullptr);
	CHECK(svc.findService<ServiceE>() == nullptr);

	CHECK_NOTHROW(svc.requestService<ServiceE>());
	CHECK(svc.findService<ServiceC>() != nullptr);
	CHECK(svc.findService<ServiceD>() != nullptr);
	CHECK(svc.findService<ServiceE>() != nullptr);
}

TEST_CASE("'ServiceLocator' failure at service startup", "[voxen::svc::service_locator]")
{
	using ServiceA = DepService<UID("1fc82db5-ea75f28a-c21c223b-10663645")>;
	using ServiceB = DepService<UID("c2b6fae1-a1aded58-0f054134-53d47bec"), ServiceA>;
	using ServiceC = DepService<UID("dc098141-b47700f8-2d43b146-c5c74611")>;
	using ServiceBad = BadService<UID("8819c518-0260c91d-db31ab20-f0daee10"), ServiceB, ServiceC>;
	using ServiceD = DepService<UID("eb934a1d-ea3777fe-8aeaf67f-13149325"), ServiceBad>;
	using ServiceE = DepService<UID("5eba2318-3dd0e03a-7101e4e9-e7b8dbea"), ServiceD>;

	ServiceLocator svc;

	CHECK_NOTHROW(svc.registerServiceFactory<ServiceA>(ServiceA::factory));
	CHECK_NOTHROW(svc.registerServiceFactory<ServiceB>(ServiceB::factory));
	CHECK_NOTHROW(svc.registerServiceFactory<ServiceC>(ServiceC::factory));
	CHECK_NOTHROW(svc.registerServiceFactory<ServiceBad>(ServiceBad::factory));
	CHECK_NOTHROW(svc.registerServiceFactory<ServiceD>(ServiceD::factory));
	CHECK_NOTHROW(svc.registerServiceFactory<ServiceE>(ServiceE::factory));

	CHECK_THROWS_WITH(svc.requestService<ServiceE>(), "oops");
	CHECK(svc.findService<ServiceA>() != nullptr);
	CHECK(svc.findService<ServiceB>() != nullptr);
	CHECK(svc.findService<ServiceC>() != nullptr);
	CHECK(svc.findService<ServiceBad>() == nullptr);
	CHECK(svc.findService<ServiceD>() == nullptr);
	CHECK(svc.findService<ServiceE>() == nullptr);
}

TEST_CASE("'ServiceLocator' double service registration", "[voxen::svc::service_locator]")
{
	using ServiceA = DepService<UID("1fc82db5-ea75f28a-c21c223b-10663645")>;
	using ServiceB = DepService<UID("c2b6fae1-a1aded58-0f054134-53d47bec"), ServiceA>;

	ServiceLocator svc;

	CHECK_NOTHROW(svc.registerServiceFactory<ServiceA>(ServiceA::factory));
	CHECK_NOTHROW(svc.registerServiceFactory<ServiceB>(ServiceB::factory));
	CHECK_THROWS_MATCHES(svc.registerServiceFactory<ServiceB>(ServiceB::factory), Exception,
		test::errcExceptionMatcher(VoxenErrc::AlreadyRegistered));

	CHECK_NOTHROW(svc.requestService<ServiceB>());
	CHECK(svc.findService<ServiceA>() != nullptr);
	CHECK(svc.findService<ServiceB>() != nullptr);

	CHECK_THROWS_MATCHES(svc.registerServiceFactory<ServiceA>(ServiceA::factory), Exception,
		test::errcExceptionMatcher(VoxenErrc::AlreadyRegistered));

	CHECK(svc.findService<ServiceA>() != nullptr);
	CHECK(svc.findService<ServiceB>() != nullptr);

	CHECK_NOTHROW(svc.requestService<ServiceB>());
	CHECK_NOTHROW(svc.requestService<ServiceA>());
}

TEST_CASE("'ServiceLocator' unresolved dependency", "[voxen::svc::service_locator]")
{
	using ServiceA = DepService<UID("1fc82db5-ea75f28a-c21c223b-10663645")>;
	using ServiceB = DepService<UID("c2b6fae1-a1aded58-0f054134-53d47bec"), ServiceA>;

	ServiceLocator svc;

	CHECK_NOTHROW(svc.registerServiceFactory<ServiceB>(ServiceB::factory));
	CHECK(svc.findService<ServiceA>() == nullptr);
	CHECK(svc.findService<ServiceB>() == nullptr);

	CHECK_THROWS_MATCHES(svc.requestService<ServiceB>(), Exception,
		test::errcExceptionMatcher(VoxenErrc::UnresolvedDependency));
	CHECK(svc.findService<ServiceA>() == nullptr);
	CHECK(svc.findService<ServiceB>() == nullptr);

	CHECK_NOTHROW(svc.registerServiceFactory<ServiceA>(ServiceA::factory));
	CHECK(svc.findService<ServiceA>() == nullptr);
	CHECK(svc.findService<ServiceB>() == nullptr);

	CHECK_NOTHROW(svc.requestService<ServiceB>());
	CHECK(svc.findService<ServiceA>() != nullptr);
	CHECK(svc.findService<ServiceB>() != nullptr);
}

TEST_CASE("'ServiceLocator' circular dependency", "[voxen::svc::service_locator]")
{
	constexpr UID UA("1fc82db5-ea75f28a-c21c223b-10663645");
	constexpr UID UB("c2b6fae1-a1aded58-0f054134-53d47bec");

	using ServiceA = DepService<UA, DepService<UB>>;
	using ServiceB = DepService<UB, DepService<UA>>;

	ServiceLocator svc;

	CHECK_NOTHROW(svc.registerServiceFactory<ServiceA>(ServiceA::factory));
	CHECK_NOTHROW(svc.registerServiceFactory<ServiceB>(ServiceB::factory));

	CHECK_THROWS_MATCHES(svc.requestService<ServiceA>(), Exception,
		test::errcExceptionMatcher(VoxenErrc::CircularDependency));
	CHECK(svc.findService<ServiceA>() == nullptr);
	CHECK(svc.findService<ServiceB>() == nullptr);

	CHECK_THROWS_MATCHES(svc.requestService<ServiceB>(), Exception,
		test::errcExceptionMatcher(VoxenErrc::CircularDependency));
	CHECK(svc.findService<ServiceA>() == nullptr);
	CHECK(svc.findService<ServiceB>() == nullptr);
}

TEST_CASE("'ServiceLocator' registering factories inside other factories", "[voxen::svc::service_locator]")
{
	using ServiceA = DepService<UID("1fc82db5-ea75f28a-c21c223b-10663645")>;
	using ServiceB = DepService<UID("c2b6fae1-a1aded58-0f054134-53d47bec")>;
	using ServiceC = DepService<UID("dc098141-b47700f8-2d43b146-c5c74611")>;
	using ServiceR = RecursiveService<UID("8819c518-0260c91d-db31ab20-f0daee10"), ServiceA, ServiceB, ServiceC>;
	using ServiceD = DepService<UID("eb934a1d-ea3777fe-8aeaf67f-13149325"), ServiceB>;
	using ServiceR2 = RecursiveService<UID("5eba2318-3dd0e03a-7101e4e9-e7b8dbea"), ServiceR, ServiceD>;

	ServiceLocator svc;

	CHECK_NOTHROW(svc.registerServiceFactory<ServiceR2>(ServiceR2::factory));

	CHECK_NOTHROW(svc.requestService<ServiceR2>());
	CHECK(svc.findService<ServiceA>() != nullptr);
	CHECK(svc.findService<ServiceB>() != nullptr);
	CHECK(svc.findService<ServiceC>() != nullptr);
	CHECK(svc.findService<ServiceR>() != nullptr);
	CHECK(svc.findService<ServiceD>() != nullptr);
	CHECK(svc.findService<ServiceR2>() != nullptr);
}

TEST_CASE("'ServiceLocator' service startup from a different thread", "[voxen::svc::service_locator]")
{
	using ServiceA = DepService<UID("1fc82db5-ea75f28a-c21c223b-10663645")>;
	using ServiceB = DepService<UID("c2b6fae1-a1aded58-0f054134-53d47bec")>;
	using ServiceC = DepService<UID("dc098141-b47700f8-2d43b146-c5c74611")>;
	using ServiceT = ThreadedService<UID("8819c518-0260c91d-db31ab20-f0daee10"), ServiceA, ServiceB, ServiceC>;

	ServiceLocator svc;

	CHECK_NOTHROW(svc.registerServiceFactory<ServiceA>(ServiceA::factory));
	CHECK_NOTHROW(svc.registerServiceFactory<ServiceB>(ServiceB::factory));
	CHECK_NOTHROW(svc.registerServiceFactory<ServiceC>(ServiceC::factory));
	CHECK_NOTHROW(svc.registerServiceFactory<ServiceT>(ServiceT::factory));

	CHECK_NOTHROW(svc.requestService<ServiceT>());
	CHECK(svc.findService<ServiceA>() != nullptr);
	CHECK(svc.findService<ServiceB>() != nullptr);
	CHECK(svc.findService<ServiceC>() != nullptr);
	CHECK(svc.findService<ServiceT>() != nullptr);
}

} // namespace voxen::svc
