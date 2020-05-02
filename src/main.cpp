#include <voxen/config.hpp>
#include <voxen/client/window.hpp>
#include <voxen/client/vulkan/vulkan_render.hpp>
#include <voxen/common/world.hpp>
#include <voxen/common/gui.hpp>
#include <voxen/util/assert.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

#include <chrono>
#include <string>

int main (int argc, char *argv[]) {
	using voxen::Log;
	using namespace std::chrono;

	Log::info("Starting Voxen {}", voxen::BuildConfig::kVersionString);
	Log::info("Commandline args:");
	for (int i = 0; i < argc; i++)
		Log::info("{} => {}", i, argv[i]);


	bool isLoggingFPSEnable = false;
	if (argc > 2 && std::string(argv[1]) == "logging_fps")
		isLoggingFPSEnable = true;

	try {
		auto &wnd = voxen::Window::instance();
		wnd.start();
		voxen::VulkanRender *render = new voxen::VulkanRender(wnd);

		voxen::World world;
		voxen::GUI gui;
		wnd.attachGUI(gui);
		gui.init(world);
		voxen::DebugQueueRtW render_to_world_queue;

		auto last_tick_time = high_resolution_clock::now();
		const duration<int64_t, std::nano> tick_inverval { int64_t(world.secondsPerTick() * 1'000'000'000.0) };
		auto next_tick_time = last_tick_time + tick_inverval;

		int64_t fps_counter = 0;
		int64_t ups_counter = 0;
		auto time_point_counter = high_resolution_clock::now();
		while (!wnd.shouldClose()) {
			if (isLoggingFPSEnable)
			{
				duration<double> dur = (high_resolution_clock::now() - time_point_counter);
				if (dur.count() > 1)
				{
					Log::info("FPS: {} UPS: {}", (int)(fps_counter/dur.count()), (int)(ups_counter/dur.count()));
					fps_counter = 0;
					ups_counter = 0;
					time_point_counter = high_resolution_clock::now();
				}
			}

			fflush(stdout);

			// World
			auto cur_time = high_resolution_clock::now();
			while (cur_time >= next_tick_time) {
				ups_counter++;
				world.update(render_to_world_queue, tick_inverval);
				next_tick_time += tick_inverval;
			}

			//GUI

			// TODO .getLastState();
			voxen::World state(world);
			// Input handle
			wnd.pollEvents();
			gui.update(state, render_to_world_queue);
			// GUI now handled a lot of callbacks (events) from Window
			// and update world via queue

			// Do render
			voxen::Player player = state.player();
			render->beginFrame();
			state.walkActiveChunks([render, player](const voxen::TerrainChunk &chunk) {
				float x = float(chunk.baseX());
				float y = float(chunk.baseY());
				float z = float(chunk.baseZ());
				float sz = float(chunk.size() * chunk.scale());
				render->debugDrawOctreeNode(player, x, y, z, sz);
			});
			render->endFrame();
			fps_counter++;
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
