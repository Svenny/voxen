#include <voxen/config.hpp>
#include <voxen/client/window.hpp>
#include <voxen/client/vulkan/vulkan_render.hpp>
#include <voxen/common/world.hpp>
#include <voxen/util/assert.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

#include <chrono>

int main (int argc, char *argv[]) {
	using voxen::Log;
	using namespace std::chrono;

	Log::info("Starting Voxen {}", voxen::BuildConfig::kVersionString);
	Log::info("Commandline args:");
	for (int i = 0; i < argc; i++)
		Log::info("{} => {}", i, argv[i]);

	try {
		auto &wnd = voxen::Window::instance();
		wnd.start();
		voxen::VulkanRender *render = new voxen::VulkanRender(wnd);

		voxen::World world;

		auto last_tick_time = high_resolution_clock::now();
		const duration<int64_t, std::nano> tick_inverval { int64_t(world.secondsPerTick() * 1'000'000'000.0) };
		auto next_tick_time = last_tick_time + tick_inverval;

		while (!wnd.shouldClose()) {
			fflush(stdout);

			auto cur_time = high_resolution_clock::now();
			while (cur_time >= next_tick_time) {
				world.update();
				next_tick_time += tick_inverval;
			}

			wnd.pollEvents();
			render->beginFrame();
			world.render(*render);
			render->endFrame();
		}

		delete render;
		wnd.stop();
	}
	catch (const voxen::Exception &e) {
		Log::fatal("Unchaught voxen::Exception instance");
		Log::fatal("what(): {}", e.what());
		auto loc = e.where();
		Log::fatal("where(): {}:{} ({})", loc.file_name(), loc.line(), loc.function_name());
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
	return 0;
}
