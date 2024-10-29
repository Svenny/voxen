#pragma once

#include <voxen/svc/service_locator.hpp>
#include <voxen/visibility.hpp>

#include <memory>

namespace voxen::svc
{

// Despite its formidable name, this class does a relatively mundane job
// of managing the startup/shutdown sequence and not much more than that.
//
// It provides service locator with registered factories for built-in
// engine services (those declared in `include/voxen...` subdirectory).
// Additional services can be registered as well.
//
// It is also responsible for receiving the set of initial settings
// via command-line arguments, config files, environment etc. and
// passing it down as service configurations.
//
// NOTE: as some engine parts are singletons, then consequently the whole
// engine has to be a singleton too. It's not possible to instantiate more
// than one `Engine` at once.
class VOXEN_API Engine {
public:
	Engine(Engine &&) = delete;
	Engine(const Engine &) = delete;
	Engine &operator=(Engine &&) = delete;
	Engine &operator=(const Engine &) = delete;
	~Engine() noexcept;

	// Factory function. Ensures no other instance is currently created,
	// otherwise throws `Exception` with `VoxenErrc::AlreadyRegistered`.
	static std::unique_ptr<Engine> create();

	// Service locator to use for this engine instance.
	// Factories for built-in services (declared in `include/voxen/...` subdirectory)
	// are already registered, you can call `requestService` for any of them.
	ServiceLocator &serviceLocator() noexcept { return m_service_locator; }

private:
	Engine();

	ServiceLocator m_service_locator;
};

} // namespace voxen::svc
