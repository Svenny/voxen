#include "game_ui.hpp"

#include <voxen/client/main_thread_service.hpp>
#include <voxen/debug/uid_registry.hpp>
#include <voxen/exception.hpp>
#include <voxen/log.hpp>
#include <voxen/svc/engine.hpp>
#include <voxen/svc/service_locator.hpp>
#include <voxen/version.hpp>
#include <voxen/world/world_control_service.hpp>

#include <extras/defer.hpp>

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <latch>
#include <memory>
#include <system_error>
#include <utility>

namespace
{

// `Engine` registers only its core services, add the higher-level ones the game uses
void registerGameServices(voxen::svc::ServiceLocator &svc)
{
	using voxen::client::MainThreadService;
	using voxen::world::ControlService;

	voxen::debug::UidRegistry::registerLiteral(MainThreadService::SERVICE_UID, "voxen::client::MainThreadService");
	voxen::debug::UidRegistry::registerLiteral(ControlService::SERVICE_UID, "voxen::world::ControlService");

	svc.registerServiceFactory<MainThreadService>([](voxen::svc::ServiceLocator &sl) {
		return std::make_unique<MainThreadService>(sl, MainThreadService::Config {});
	});
	svc.registerServiceFactory<ControlService>(
		[](voxen::svc::ServiceLocator &sl) { return std::make_unique<ControlService>(sl); });
}

} // namespace

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
		registerGameServices(engine->serviceLocator());

		// We don't have UI and save/load yet, so just create a new world.
		auto &world_control = engine->serviceLocator().requestService<voxen::world::ControlService>();

		{
			std::latch latch(1);

			world_control.asyncStartWorld({
				// Don't load any saved world
				.storage_directory = {},
				// Just report it, but we could also update progress bar in UI
				.progress_callback =
					[](float progress) { Log::info("World starting progress: {:.0f}%", progress * 100.0f); },
				.result_callback =
					[&latch](std::error_condition error) {
						if (error) {
							// TODO: any handling actions?
							Log::error("World start failed: {} ([{}: {}])", error.message(), error.category().name(),
								error.value());
						}

						latch.count_down();
					},
			});

			// Block until the world starts
			latch.wait();
		}

		// Stop the world on any exit from this scope, including an exception from the main loop.
		// Services it runs on (land, tasks etc.) are destroyed before `ControlService` itself,
		// so it must stop before the engine is destroyed.
		defer {
			std::latch latch(1);

			world_control.asyncStopWorld({
				// Just report it, but we could also update progress bar in UI
				.progress_callback = [](float progress) { Log::info("World saving progress: {:.0f}%", progress * 100.0f); },
				.result_callback =
					[&latch](std::error_condition error) {
						if (error) {
							// TODO: any handling actions?
							Log::error("World stop failed: {} ([{}: {}])", error.message(), error.category().name(),
								error.value());
						}

						latch.count_down();
					},
			});

			// Block until the world stops
			latch.wait();
		};

		vxgame::Ui ui;

		auto &main_thread = engine->serviceLocator().requestService<voxen::client::MainThreadService>();
		// Will stay inside this function until the game is ordered to exit
		main_thread.doMainLoop([&](voxen::client::MainThreadService::FrameCallbackData fcd) {
			ui.draw(fcd.ui_builder);
			return true;
		});
	}
	catch (const voxen::Exception &e) {
		Log::fatal("Uncaught voxen::Exception instance");
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
