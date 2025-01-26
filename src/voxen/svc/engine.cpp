#include <voxen/svc/engine.hpp>

#include <voxen/client/main_thread_service.hpp>
#include <voxen/common/config.hpp>
#include <voxen/common/filemanager.hpp>
#include <voxen/common/pipe_memory_allocator.hpp>
#include <voxen/common/runtime_config.hpp>
#include <voxen/debug/uid_registry.hpp>
#include <voxen/server/world.hpp>
#include <voxen/svc/async_file_io_service.hpp>
#include <voxen/svc/messaging_service.hpp>
#include <voxen/svc/task_service.hpp>
#include <voxen/util/error_condition.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>
#include <voxen/version.hpp>

#include "async_counter_tracker.hpp"

#include <fmt/ranges.h>

#include <atomic>

namespace voxen::svc
{

namespace
{

const std::string CLI_SECTION_SEPARATOR = "__";

cxxopts::Options makeCliOptions()
{
	cxxopts::Options options("voxen", "Voxen - awesome VOXel ENgine");
	Config::Scheme scheme = Config::mainConfigScheme();

	for (Config::SchemeEntry &entry : scheme) {
		std::shared_ptr<cxxopts::Value> default_cli_value;
		switch (entry.default_value.index()) {
		case 0:
			static_assert(std::is_same_v<std::string, std::variant_alternative_t<0, Config::option_t>>);
			default_cli_value = cxxopts::value<std::string>();
			break;

		case 1:
			static_assert(std::is_same_v<int64_t, std::variant_alternative_t<1, Config::option_t>>);
			default_cli_value = cxxopts::value<int>();
			break;

		case 2:
			static_assert(std::is_same_v<double, std::variant_alternative_t<2, Config::option_t>>);
			default_cli_value = cxxopts::value<double>();
			break;

		case 3:
			static_assert(std::is_same_v<bool, std::variant_alternative_t<3, Config::option_t>>);
			// Minor UX convenience. This allows use bool flag like `--dev__fps_logging` instead of strict `--dev__fps_loggin=true` form
			default_cli_value = cxxopts::value<bool>()->default_value("true");
			break;

		default:
			static_assert(std::variant_size_v<Config::option_t> == 4);
			break;
		}

		options.add_options(entry.section)(entry.section + CLI_SECTION_SEPARATOR + entry.parameter_name,
			entry.description, default_cli_value);
	}

	// clang-format off: breaks nice chaining syntax
	options.add_options()
		("h,help", "Display help information")
		("p,profile", "Profile name", cxxopts::value<std::string>()->default_value("default"));
	// clang-format on

	RuntimeConfig::addOptions(options);

	return options;
}

void patchConfig(const cxxopts::ParseResult &result, Config *config)
{
	for (const auto &keyvalue : result.arguments()) {
		size_t sep_idx = keyvalue.key().find(CLI_SECTION_SEPARATOR);
		if (sep_idx == std::string::npos) {
			continue;
		}

		std::string section = keyvalue.key().substr(0, sep_idx);
		std::string parameter = keyvalue.key().substr(sep_idx + CLI_SECTION_SEPARATOR.size());

		config->patch(section, parameter, keyvalue.value());
	}
}

std::string makeVersionString(int32_t major, int32_t minor, int32_t patch, std::string_view appendix,
	std::string_view git_hash)
{
	std::string text = fmt::format("{}.{}.{}", major, minor, patch);

	if (!appendix.empty()) {
		text += ' ';
		text += appendix;
	}

	if (!git_hash.empty()) {
		text += " (git ";
		text += git_hash;
		text += ')';
	}

	return text;
}

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

auto makeAsyncFileIoService(ServiceLocator &svc)
{
	return std::make_unique<AsyncFileIoService>(svc, AsyncFileIoService::Config {});
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

// Since it's singleton we can use fixed storage
alignas(Engine) std::byte g_engine_storage[sizeof(Engine)];

} // namespace

EngineStartArgs::EngineStartArgs(AppInfo app_info) : m_app_info(std::move(app_info)) {}

EngineStartArgs::EngineStartArgs(EngineStartArgs &&) noexcept = default;
EngineStartArgs &EngineStartArgs::operator=(EngineStartArgs &&) noexcept = default;
EngineStartArgs::~EngineStartArgs() = default;

auto EngineStartArgs::fillFromArgv(int argc, const char *const *argv) -> ArgvParseResult
{
	cxxopts::Options opts = makeCliOptions();

	try {
		m_argv0 = argv[0];
		m_parsed_cli_opts = opts.parse(argc, argv);

		if (m_parsed_cli_opts.count("help")) {
			return { ArgvParseStatus::HelpRequested, opts.help() };
		}

		const auto &unmatched = m_parsed_cli_opts.unmatched();

		if (!unmatched.empty()) {
			std::string help_text = fmt::format("Unknown arguments provided:\n{}", unmatched);
			return { ArgvParseStatus::InvalidArguments, std::move(help_text) };
		}

		return { ArgvParseStatus::Success, "" };
	}
	catch (cxxopts::exceptions::exception &ex) {
		std::string help_text
			= fmt::format("Invalid options provided, use -h (--help) to get usage help.\nError details:\n{}", ex.what());
		return { ArgvParseStatus::InvalidArguments, std::move(help_text) };
	}
}

void Engine::Deleter::operator()(Engine *engine) noexcept
{
	engine->~Engine();
	// Allow creating another instance after destroying this one
	g_instance_created.store(false);
}

Engine::Engine(EngineStartArgs args) : m_start_args(std::move(args))
{
	const AppInfo app_info = m_start_args.appInfo();

	Log::info("Started at: {:%c UTC%z (%Z)}", fmt::localtime(std::time(nullptr)));
	Log::info("Engine: Voxen {}",
		makeVersionString(Version::MAJOR, Version::MINOR, Version::PATCH, Version::SUFFIX, Version::GIT_HASH));
	Log::info("Application: {} {}", app_info.name,
		makeVersionString(app_info.version_major, app_info.version_minor, app_info.version_patch,
			app_info.version_appendix, app_info.git_commit_hash));

	const cxxopts::ParseResult &cli_opts = m_start_args.parsedCliOpts();
	RuntimeConfig::instance().fill(cli_opts);

	// Don't do anything with configs/files when launched from test suite
	// TODO: we should actually do it to support file operations in tests.
	// The application (engine creator) should decide whether to use files or not.
	// TODO: detect test suite launch in a more appropriate way
	if (!m_start_args.argv0().empty()) {
		// TODO: convert `FileManager` to a service
		voxen::FileManager::setProfileName(m_start_args.argv0(), cli_opts["profile"].as<std::string>());
		// TODO: this is some old temporary junk, remove it
		patchConfig(cli_opts, Config::mainConfig());
	}

	debug::UidRegistry::registerLiteral(MessagingService::SERVICE_UID, "voxen::svc::MessagingService");
	debug::UidRegistry::registerLiteral(PipeMemoryAllocator::SERVICE_UID, "voxen::PipeMemoryAllocator");
	debug::UidRegistry::registerLiteral(detail::AsyncCounterTracker::SERVICE_UID,
		"voxen::svc::detail::AsyncCounterTracker");
	debug::UidRegistry::registerLiteral(TaskService::SERVICE_UID, "voxen::svc::TaskService");
	debug::UidRegistry::registerLiteral(AsyncFileIoService::SERVICE_UID, "voxen::svc::AsyncFileIoService");
	debug::UidRegistry::registerLiteral(client::MainThreadService::SERVICE_UID, "voxen::client::MainThreadService");
	debug::UidRegistry::registerLiteral(server::World::SERVICE_UID, "voxen::server::World");

	m_service_locator.registerServiceFactory<MessagingService>(makeMsgService);
	m_service_locator.registerServiceFactory<PipeMemoryAllocator>(makePipeAllocService);
	m_service_locator.registerServiceFactory<detail::AsyncCounterTracker>(makeAsyncCounterTracker);
	m_service_locator.registerServiceFactory<TaskService>(makeTaskService);
	m_service_locator.registerServiceFactory<AsyncFileIoService>(makeAsyncFileIoService);
	m_service_locator.registerServiceFactory<client::MainThreadService>(makeMainThreadService);
	m_service_locator.registerServiceFactory<server::World>(makeWorldService);
}

Engine::~Engine() = default;

auto Engine::create(EngineStartArgs args) -> Ptr
{
	if (g_instance_created.exchange(true) == true) {
		Log::error("Engine instance is already created!");
		throw Exception::fromError(VoxenErrc::AlreadyRegistered, "Engine singleton violated");
	}

	try {
		return Ptr(new (g_engine_storage) Engine(std::move(args)));
	}
	catch (...) {
		// Engine ctor has thrown - unset created flag manually, deleter will not be run
		g_instance_created.store(false);
		throw;
	}
}

auto Engine::createForTestSuite() -> Ptr
{
	// Test suite shares repository with the engine,
	// hence it has the same versioning and commit hash
	EngineStartArgs args(AppInfo {
		.name = "Voxen Test Suite",
		.version_major = Version::MAJOR,
		.version_minor = Version::MINOR,
		.version_patch = Version::PATCH,
		.version_appendix = Version::SUFFIX,
		.git_commit_hash = Version::GIT_HASH,
	});

	// Communicate that we're launched from test suite (see `Engine` ctor)
	// TODO: this is sooo hacky
	const char *argv0 = "";
	args.fillFromArgv(1, &argv0);

	return create(std::move(args));
}

} // namespace voxen::svc
