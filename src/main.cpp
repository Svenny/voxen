#include <voxen/config.hpp>
#include <voxen/client/window.hpp>
#include <voxen/client/vulkan/vulkan_render.hpp>
#include <voxen/common/world.hpp>
#include <voxen/common/gui.hpp>
#include <voxen/common/config.hpp>
#include <voxen/util/assert.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

#include <cxxopts.hpp>

#include <chrono>
#include <string>
#include <algorithm>
#include <variant>

static const std::string kCliSectionSeparator = "__";

cxxopts::Options initCli() {
	using namespace voxen;

	cxxopts::Options options("voxen", "VOXEN - avesome VOXel ENgine and game");
	Config::Scheme scheme = Config::mainConfigScheme();

	for(Config::SchemeEntry& entry : scheme) {
		const std::string& arg_name = entry.section + kCliSectionSeparator + entry.parameter_name;

		std::shared_ptr<cxxopts::Value> default_cli_value;
		switch(entry.default_value.index())
		{
			case 0:
				static_assert(std::is_same_v<std::string,   std::variant_alternative_t<0, voxen::Config::option_t>>);
				default_cli_value = cxxopts::value<std::string>();
				break;

			case 1:
				static_assert(std::is_same_v<int64_t,   std::variant_alternative_t<1, voxen::Config::option_t>>);
				default_cli_value = cxxopts::value<int>();
				break;

			case 2:
				static_assert(std::is_same_v<double,   std::variant_alternative_t<2, voxen::Config::option_t>>);
				default_cli_value = cxxopts::value<double>();
				break;

			case 3:
				static_assert(std::is_same_v<bool,   std::variant_alternative_t<3, voxen::Config::option_t>>);
				// Minor UX convenience. This allows use bool flag like `--dev__fps_logging` instead of strict `--dev__fps_loggin=true` form
				default_cli_value = cxxopts::value<bool>()->default_value("true");
				break;

			default:
				static_assert(std::variant_size_v<voxen::Config::option_t> == 4);
				break;
		}
		options.add_options(entry.section)
			(arg_name, entry.description, default_cli_value);
	}
	options.add_options()
		("h,help", "Display help information");
	return options;
}

void patchConfig(cxxopts::ParseResult result, voxen::Config* config) {
	for (auto& keyvalue : result.arguments()) {
		int sep_idx = keyvalue.key().find(kCliSectionSeparator);
		std::string section = keyvalue.key().substr(0, sep_idx);
		std::string parameter = keyvalue.key().substr(sep_idx+kCliSectionSeparator.size());

		int type_idx = config->optionType(section, parameter);
		voxen::Config::option_t value = voxen::Config::optionFromString(keyvalue.value(), type_idx);
		config->patch(section, parameter, value);
	}
}

int main (int argc, char *argv[]) {
	using voxen::Log;
	using namespace std::chrono;

	Log::info("Starting Voxen {}", voxen::BuildConfig::kVersionString);

	try {
		cxxopts::Options options = initCli();
		auto result = options.parse(argc, argv);
		if (result.count("help")) {
			std::cout << options.help() << std::endl;
			exit(0);
		}

		voxen::Config* main_voxen_config = voxen::Config::mainConfig();
		patchConfig(result, main_voxen_config);

		bool isLoggingFPSEnable = main_voxen_config->optionBool("dev", "fps_logging");

		auto &wnd = voxen::client::Window::instance();
		wnd.start(main_voxen_config->optionInt("window", "width"), main_voxen_config->optionInt("window", "height"));
		auto *render = new voxen::client::VulkanRender(wnd);

		voxen::World world;
		voxen::Gui gui(wnd);
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
			glm::mat4 camera_matrix = gui.view().cameraMatrix();
			render->beginFrame();
			state.walkActiveChunks([render, camera_matrix](const voxen::TerrainChunk &chunk) {
				float x = float(chunk.baseX());
				float y = float(chunk.baseY());
				float z = float(chunk.baseZ());
				float sz = float(chunk.size() * chunk.scale());
				render->debugDrawOctreeNode(camera_matrix, x, y, z, sz);
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
