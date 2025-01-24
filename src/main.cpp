#include <voxen/client/main_thread_service.hpp>
#include <voxen/server/world.hpp>
#include <voxen/svc/engine.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>
#include <voxen/version.hpp>

int main(int argc, char *argv[])
{
	using voxen::Log;

	try {
		using ArgvStatus = voxen::svc::EngineStartArgs::ArgvParseStatus;

		voxen::svc::EngineStartArgs engine_args(voxen::svc::AppInfo {
			.name = "Voxen Sample Game",
			.version_major = voxen::Version::MAJOR,
			.version_minor = voxen::Version::MINOR,
			.version_patch = voxen::Version::PATCH,
			.version_appendix = voxen::Version::SUFFIX,
			.git_commit_hash = voxen::Version::GIT_HASH,
		});

		if (auto result = engine_args.fillFromArgv(argc, argv); result.status != ArgvStatus::Success) {
			printf("%s\n", result.help_text.c_str());
			// Explicitly requested help - success; otherwise it's a failure (wrong CLI usage)
			return result.status == ArgvStatus::HelpRequested ? EXIT_SUCCESS : EXIT_FAILURE;
		}

		auto engine = voxen::svc::Engine::create(std::move(engine_args));

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

	Log::info("Exiting normally");
	return EXIT_SUCCESS;
}
