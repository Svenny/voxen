#include <voxen/client/main_thread_service.hpp>
#include <voxen/common/config.hpp>
#include <voxen/common/filemanager.hpp>
#include <voxen/common/runtime_config.hpp>
#include <voxen/common/world_state.hpp>
#include <voxen/os/glfw_window.hpp>
#include <voxen/server/world.hpp>
#include <voxen/svc/engine.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>
#include <voxen/version.hpp>

#include <extras/defer.hpp>

#include <cxxopts/cxxopts.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <set>
#include <string>
#include <thread>
#include <variant>

static const std::string kCliSectionSeparator = "__";

static cxxopts::Options initCli()
{
	using namespace voxen;

	cxxopts::Options options("voxen", "VOXEN - avesome VOXel ENgine and game");
	Config::Scheme scheme = Config::mainConfigScheme();

	for (Config::SchemeEntry &entry : scheme) {
		const std::string &arg_name = entry.section + kCliSectionSeparator + entry.parameter_name;

		std::shared_ptr<cxxopts::Value> default_cli_value;
		switch (entry.default_value.index()) {
		case 0:
			static_assert(std::is_same_v<std::string, std::variant_alternative_t<0, voxen::Config::option_t>>);
			default_cli_value = cxxopts::value<std::string>();
			break;

		case 1:
			static_assert(std::is_same_v<int64_t, std::variant_alternative_t<1, voxen::Config::option_t>>);
			default_cli_value = cxxopts::value<int>();
			break;

		case 2:
			static_assert(std::is_same_v<double, std::variant_alternative_t<2, voxen::Config::option_t>>);
			default_cli_value = cxxopts::value<double>();
			break;

		case 3:
			static_assert(std::is_same_v<bool, std::variant_alternative_t<3, voxen::Config::option_t>>);
			// Minor UX convenience. This allows use bool flag like `--dev__fps_logging` instead of strict `--dev__fps_loggin=true` form
			default_cli_value = cxxopts::value<bool>()->default_value("true");
			break;

		default:
			static_assert(std::variant_size_v<voxen::Config::option_t> == 4);
			break;
		}
		options.add_options(entry.section)(arg_name, entry.description, default_cli_value);
	}

	// clang-format off: breaks nice chaining syntax
	options.add_options()
		("h,help", "Display help information")
		("p,profile", "Profile name", cxxopts::value<std::string>()->default_value("default"));
	// clang-format on

	voxen::RuntimeConfig::addOptions(options);

	return options;
}

static void patchConfig(const cxxopts::ParseResult &result, voxen::Config *config)
{
	for (const auto &keyvalue : result.arguments()) {
		size_t sep_idx = keyvalue.key().find(kCliSectionSeparator);
		if (sep_idx == std::string::npos) {
			continue;
		}

		std::string section = keyvalue.key().substr(0, sep_idx);
		std::string parameter = keyvalue.key().substr(sep_idx + kCliSectionSeparator.size());

		config->patch(section, parameter, keyvalue.value());
	}
}

int main(int argc, char *argv[])
{
	using voxen::Log;
	using namespace std::chrono;

	Log::info("Starting Voxen {}", voxen::Version::STRING);
	Log::info("Started at: {:%c UTC%z (%Z)}", fmt::localtime(std::time(nullptr)));

	try {
		cxxopts::Options options = initCli();
		auto result = options.parse(argc, argv);
		if (result.count("help")) {
			std::cout << options.help() << std::endl;
			return 0;
		}

		voxen::RuntimeConfig::instance().fill(result);

		auto engine = voxen::svc::Engine::create();

		voxen::FileManager::setProfileName(argv[0], result["profile"].as<std::string>());

		voxen::Config *main_voxen_config = voxen::Config::mainConfig();
		patchConfig(result, main_voxen_config);

		// This will start world thread automatically
		engine->serviceLocator().requestService<voxen::server::World>();

		auto &main_thread = engine->serviceLocator().requestService<voxen::client::MainThreadService>();
		// Will stay inside this function until the game is ordered to exit
		main_thread.doMainLoop();
	}
	catch (const voxen::Exception &e) {
		Log::fatal("Unchaught voxen::Exception instance");
		Log::fatal("what(): {}", e.what());
		auto loc = e.where();
		Log::fatal("where(): {}:{}", loc.file_name(), loc.line());
		Log::fatal("Aborting the program");
		return EXIT_FAILURE;
	}
	catch (const std::exception &e) {
		Log::fatal("Uncaught std::exception instance");
		Log::fatal("what(): {}", e.what());
		Log::fatal("Aborting the program");
		return EXIT_FAILURE;
	}
	catch (...) {
		Log::fatal("Uncaught exception of unknown type");
		Log::fatal("Aborting the program");
		return EXIT_FAILURE;
	}

	Log::info("Exiting");
	return EXIT_SUCCESS;
}
