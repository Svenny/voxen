#pragma once

#include <voxen/svc/service_locator.hpp>
#include <voxen/visibility.hpp>

#include <cxxopts/cxxopts.hpp>

#include <memory>
#include <string>

namespace voxen::svc
{

// Basic information about the application starting the `Engine`.
// The full version string in debug logs will be composed following this form:
//     <name> <version_major>.<version_minor>.<version_patch> [version_appendix] [(git git_commit_hash)]
struct VOXEN_API AppInfo {
	// ASCII name of the application. It is needed mainly for logging/debugging
	// but is also passed to the graphics API driver to use per-application hacks
	// (though I sincerely hope with this engine none will be needed!)
	// You are strongly encouraged to set this name once for a project and keep it fixed.
	std::string name = "";

	// Major version of the application. No semantic versioning requirements.
	// However, it is recommended to bump it for releases containing
	// significant changes to the behavior or large content additions.
	//
	// Passed to the graphics API driver, should be in range [0; 255]
	// or the passed value will overflow and look like another version.
	int32_t version_major = 0;
	// Minor version of the application. No semantic versioning requirements.
	// However, it is recommended to bump it for releases having behavior
	// changes or content additions (i.e. not only bugfixes) and reset
	// to zero when major component is changed.
	//
	// Passed to the graphics API driver, should be in range [0; 4095]
	// or the passed value will overflow and look like another version.
	int32_t version_minor = 0;
	// Patch (subminor/bugfix/etc.) version of the application.
	// No semantic versioning requirements. However, it is recommended to bump
	// it for every new shipped build and reset to zero when minor/major component
	// is changed. This way all shipped builds will have unique versions.
	//
	// Passed to the graphics API driver, should be in range [0; 4095]
	// or the passed value will overflow and look like another version.
	int32_t version_patch = 0;

	// Arbitrary ASCII text that can store some additional information about
	// this application build that is can't be conveyed with version numbers alone.
	// For example, it might be "alpha", "beta", "debug", "experimental", "asan"
	// or even a task tracker number for builds related to a particular task.
	//
	// Used purely for logging/debugging. You can leave this string empty.
	// It is recommended to leave it empty in production (shipping) builds
	// and set to something descriptive in all other ones.
	std::string version_appendix = "";
	// Arbitrary ASCII text that should be the Git commit hash which contained
	// the source for this application build. To shorten the full version string
	// it can be reduced to several first hex digits (12-16 should be enough).
	// It is also a good practice to add "-dirty" after the hash to indicate that
	// source tree is not clean (i.e. there are some uncommitted local changes).
	//
	// Used purely for logging/debugging.
	// You can set it to something like "unknown" to indicate the commit could not
	// be determined (e.g. the source was downloaded directly and has no `.git` directory),
	// or leave empty to avoid any mention of Git (e.g. if you use another version control).
	std::string git_commit_hash = "";
};

class VOXEN_API EngineStartArgs {
public:
	enum class ArgvParseStatus {
		Success,
		HelpRequested,
		InvalidArguments,
	};

	struct ArgvParseResult {
		ArgvParseStatus status;
		std::string help_text;
	};

	EngineStartArgs(AppInfo app_info);
	EngineStartArgs(EngineStartArgs &&) noexcept;
	EngineStartArgs(const EngineStartArgs &) = delete;
	EngineStartArgs &operator=(EngineStartArgs &&) noexcept;
	EngineStartArgs &operator=(const EngineStartArgs &) = delete;
	~EngineStartArgs();

	ArgvParseResult fillFromArgv(int argc, const char *const *argv);

	AppInfo &appInfo() noexcept { return m_app_info; }
	std::string &argv0() noexcept { return m_argv0; }
	cxxopts::ParseResult &parsedCliOpts() noexcept { return m_parsed_cli_opts; }

private:
	AppInfo m_app_info;
	std::string m_argv0;
	cxxopts::ParseResult m_parsed_cli_opts;
};

// Despite its formidable name, this class does a relatively mundane job
// of managing the startup/shutdown sequence and not much more than that.
//
// It provides service locator with registered factories for built-in
// engine services (those declared in `include/voxen...` subdirectory).
// Additional services can be registered after creation as well.
// Engine does not start any services (except maybe debug ones) by itself.
//
// It is also responsible for receiving the set of initial settings
// via command-line arguments, config files, environment etc. and
// passing it down as service configurations.
//
// NOTE: as some engine parts are singletons, then consequently the whole
// engine has to be a singleton too. It's not possible to instantiate more
// than one `Engine` at once.
class VOXEN_API Engine {
private:
	struct VOXEN_API Deleter {
		void operator()(Engine *engine) noexcept;
	};

public:
	using Ptr = std::unique_ptr<Engine, Deleter>;

	Engine(Engine &&) = delete;
	Engine(const Engine &) = delete;
	Engine &operator=(Engine &&) = delete;
	Engine &operator=(const Engine &) = delete;
	~Engine();

	// Factory function. Ensures no other instance is currently created,
	// otherwise throws `Exception` with `VoxenErrc::AlreadyRegistered`.
	static Ptr create(EngineStartArgs args);
	// Same as `create()` but creates an instance tailored for test cases execution.
	// You should not call it from regular applications.
	static Ptr createForTestSuite();

	// Service locator to use for this engine instance.
	// Factories for built-in services (declared in `include/voxen/...` subdirectory)
	// are already registered, you can call `requestService` for any of them.
	ServiceLocator &serviceLocator() noexcept { return m_service_locator; }

private:
	Engine(EngineStartArgs args);

	EngineStartArgs m_start_args;
	ServiceLocator m_service_locator;
};

} // namespace voxen::svc
