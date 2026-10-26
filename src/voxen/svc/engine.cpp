#include <voxen/svc/engine.hpp>

#include <voxen/common/config.hpp>
#include <voxen/common/filemanager.hpp>
#include <voxen/common/runtime_config.hpp>
#include <voxen/debug/uid_registry.hpp>
#include <voxen/os/time.hpp>
#include <voxen/svc/async_file_io_service.hpp>
#include <voxen/svc/messaging_service.hpp>
#include <voxen/svc/pipe_memory_allocator.hpp>
#include <voxen/svc/task_service.hpp>
#include <voxen/util/error_condition.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>
#include <voxen/version.hpp>

#include "async_counter_tracker.hpp"

#include <tk/cli.hpp>

#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace voxen::svc
{

namespace
{

const std::string CLI_SECTION_SEPARATOR = "__";

tk::cli::Parser makeCliParser()
{
	tk::cli::Parser parser("voxen", "Voxen - awesome VOXel ENgine");

	parser.addFlag("help", 'h', "Display help information");
	parser.addOption({
		.name = "profile",
		.short_name = 'p',
		.type = tk::cli::Parser::ValueType::String,
		.description = "Profile name",
		.default_value = "default",
		.group = {},
	});

	for (const Config::SchemeEntry &entry : Config::mainConfigScheme()) {
		tk::cli::Parser::ValueType type = tk::cli::Parser::ValueType::String;

		switch (entry.default_value.index()) {
		case 0:
			static_assert(std::is_same_v<std::string, std::variant_alternative_t<0, Config::option_t>>);
			type = tk::cli::Parser::ValueType::String;
			break;

		case 1:
			static_assert(std::is_same_v<int64_t, std::variant_alternative_t<1, Config::option_t>>);
			type = tk::cli::Parser::ValueType::Integer;
			break;

		case 2:
			static_assert(std::is_same_v<double, std::variant_alternative_t<2, Config::option_t>>);
			type = tk::cli::Parser::ValueType::Float;
			break;

		case 3:
			static_assert(std::is_same_v<bool, std::variant_alternative_t<3, Config::option_t>>);
			// Minor UX convenience. This allows use bool flag like `--dev__fps_logging` instead of strict `--dev__fps_loggin=true` form
			type = tk::cli::Parser::ValueType::Flag;
			break;

		default:
			static_assert(std::variant_size_v<Config::option_t> == 4);
			break;
		}

		parser.addOption({
			.name = entry.section + CLI_SECTION_SEPARATOR + entry.parameter_name,
			.short_name = 0,
			.type = type,
			.description = entry.description,
			.default_value = std::nullopt,
			.group = entry.section,
		});
	}

	RuntimeConfig::addOptions(parser);

	return parser;
}

void patchConfig(const tk::cli::Args &args, Config *config)
{
	for (const tk::cli::Args::Entry &entry : args.entries()) {
		size_t sep_idx = entry.name.find(CLI_SECTION_SEPARATOR);
		if (sep_idx == std::string::npos) {
			continue;
		}

		std::string section = entry.name.substr(0, sep_idx);
		std::string parameter = entry.name.substr(sep_idx + CLI_SECTION_SEPARATOR.size());

		config->patch(section, parameter, entry.value);
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
	tk::cli::Parser parser = makeCliParser();

	m_argv0 = argv[0];
	auto result = parser.parse(argc, argv);

	if (!result) {
		std::string help_text
			= fmt::format("Invalid options provided, use -h (--help) to get usage help.\nError details:\n{}",
				result.error());
		return { ArgvParseStatus::InvalidArguments, std::move(help_text) };
	}

	if (result->flag("help")) {
		return { ArgvParseStatus::HelpRequested, parser.helpText() };
	}

	if (!result->positional().empty()) {
		std::string help_text = fmt::format("Unknown arguments provided:\n{}", result->positional());
		return { ArgvParseStatus::InvalidArguments, std::move(help_text) };
	}

	m_parsed_cli_opts = std::make_unique<tk::cli::Args>(std::move(*result));
	return { ArgvParseStatus::Success, "" };
}

const tk::cli::Args &EngineStartArgs::parsedCliOpts() const noexcept
{
	// Not filled (or moved-from) instance has no parse result
	static const tk::cli::Args EMPTY;
	return m_parsed_cli_opts ? *m_parsed_cli_opts : EMPTY;
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

	const os::Time::LocalTime start_time = os::Time::localTime(std::time(nullptr));
	const int32_t utc_offset_abs = std::abs(start_time.utc_offset_minutes);
	Log::info("Started at: {:%c} UTC{}{:02}{:02} ({})", start_time.tm, start_time.utc_offset_minutes < 0 ? '-' : '+',
		utc_offset_abs / 60, utc_offset_abs % 60, start_time.zone_name);
	Log::info("Engine: Voxen {}",
		makeVersionString(Version::MAJOR, Version::MINOR, Version::PATCH, Version::SUFFIX, Version::GIT_HASH));
	Log::info("Application: {} {}", app_info.name,
		makeVersionString(app_info.version_major, app_info.version_minor, app_info.version_patch,
			app_info.version_appendix, app_info.git_commit_hash));

	const tk::cli::Args &cli_opts = m_start_args.parsedCliOpts();
	RuntimeConfig::instance().fill(cli_opts);

	// Profile and config files are located relative to the binary, which is known only
	// when started with a command line. Without it (e.g. in tests) don't touch any files.
	if (!m_start_args.argv0().empty()) {
		// TODO: convert `FileManager` to a service
		voxen::FileManager::setProfileName(m_start_args.argv0(),
			std::string(cli_opts.value("profile").value_or("default")));
		// TODO: this is some old temporary junk, remove it
		patchConfig(cli_opts, Config::mainConfig());
	}

	debug::UidRegistry::registerLiteral(MessagingService::SERVICE_UID, "voxen::svc::MessagingService");
	debug::UidRegistry::registerLiteral(PipeMemoryAllocator::SERVICE_UID, "voxen::svc::PipeMemoryAllocator");
	debug::UidRegistry::registerLiteral(detail::AsyncCounterTracker::SERVICE_UID,
		"voxen::svc::detail::AsyncCounterTracker");
	debug::UidRegistry::registerLiteral(TaskService::SERVICE_UID, "voxen::svc::TaskService");
	debug::UidRegistry::registerLiteral(AsyncFileIoService::SERVICE_UID, "voxen::svc::AsyncFileIoService");

	m_service_locator.registerServiceFactory<MessagingService>(makeMsgService);
	m_service_locator.registerServiceFactory<PipeMemoryAllocator>(makePipeAllocService);
	m_service_locator.registerServiceFactory<detail::AsyncCounterTracker>(makeAsyncCounterTracker);
	m_service_locator.registerServiceFactory<TaskService>(makeTaskService);
	m_service_locator.registerServiceFactory<AsyncFileIoService>(makeAsyncFileIoService);
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

} // namespace voxen::svc
