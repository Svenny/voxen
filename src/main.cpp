#include <voxen/config.hpp>
#include <voxen/client/gui.hpp>
#include <voxen/client/render.hpp>
#include <voxen/client/window.hpp>
#include <voxen/common/world_state.hpp>
#include <voxen/common/config.hpp>
#include <voxen/server/world.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>
#include <voxen/common/filemanager.hpp>
#include <voxen/common/threadpool.hpp>

#include <cxxopts/cxxopts.hpp>

#include <atomic>
#include <chrono>
#include <string>
#include <set>
#include <algorithm>
#include <variant>
#include <thread>

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
		("h,help", "Display help information")
		("p,profile", "Profile name", cxxopts::value<std::string>()->default_value("default"));
	return options;
}

static std::set<std::string> ignorable_keys = {"profile"};
void patchConfig(const cxxopts::ParseResult &result, voxen::Config* config) {
	for (const auto& keyvalue : result.arguments()) {
		if (ignorable_keys.count(keyvalue.key())) {
			continue;
		}

		size_t sep_idx = keyvalue.key().find(kCliSectionSeparator);
		std::string section = keyvalue.key().substr(0, sep_idx);
		std::string parameter = keyvalue.key().substr(sep_idx+kCliSectionSeparator.size());

		size_t type_idx = config->optionType(section, parameter);
		voxen::Config::option_t value = voxen::Config::optionFromString(keyvalue.value(), type_idx);
		config->patch(section, parameter, value);
	}
}

// TODO: use queue or some "inter-thread data exchange" object
std::atomic_bool g_stop { false };
std::atomic_int64_t g_ups_counter { 0 };

void worldThread(voxen::server::World &world, voxen::DebugQueueRtW &render_to_world_queue)
{
	using namespace std::chrono;

	auto last_tick_time = high_resolution_clock::now();
	const duration<int64_t, std::nano> tick_inverval { int64_t(world.secondsPerTick() * 1'000'000'000.0) };
	auto next_tick_time = last_tick_time + tick_inverval;

	while (!g_stop.load()) {
		auto cur_time = high_resolution_clock::now();
		while (cur_time >= next_tick_time) {
			world.update(render_to_world_queue, tick_inverval);
			next_tick_time += tick_inverval;
			g_ups_counter.fetch_add(1, std::memory_order_relaxed);
			if (g_stop.load()) {
				break;
			}
		}
		std::this_thread::sleep_until(next_tick_time - milliseconds(1));
	}
}

int main(int argc, char *argv[])
{
	using voxen::Log;
	using namespace std::chrono;

	Log::info("Starting Voxen {}", voxen::BuildConfig::kVersionString);

	try {
		cxxopts::Options options = initCli();
		auto result = options.parse(argc, argv);
		if (result.count("help")) {
			std::cout << options.help() << std::endl;
			return 0;
		}

		//TODO(sirgienko) add profile option for threads count in profile scheme?
		voxen::ThreadPool::initGlobalVoxenPool();

		voxen::FileManager::setProfileName(argv[0], result["profile"].as<std::string>());

		voxen::Config* main_voxen_config = voxen::Config::mainConfig();
		patchConfig(result, main_voxen_config);

		bool isLoggingFPSEnable = main_voxen_config->optionBool("dev", "fps_logging");

		auto &wnd = voxen::client::Window::instance();
		wnd.start(main_voxen_config->optionInt32("window", "width"), main_voxen_config->optionInt32("window", "height"));
		auto render = std::make_unique<voxen::client::Render>(wnd);

		voxen::server::World world;
		auto gui = std::make_unique<voxen::client::Gui>(wnd);
		gui->init(*world.getLastState());
		voxen::DebugQueueRtW render_to_world_queue;

		std::thread world_thread(worldThread, std::ref(world), std::ref(render_to_world_queue));

		int64_t fps_counter = 0;
		auto time_point_counter = high_resolution_clock::now();
		while (!wnd.shouldClose()) {
			// Write all possibly buffered log messages
			fflush(stdout);

			if (isLoggingFPSEnable) {
				duration<double> dur = (high_resolution_clock::now() - time_point_counter);
				double elapsed = dur.count();
				if (elapsed > 2) {
					int64_t ups_counter = g_ups_counter.exchange(0, std::memory_order_relaxed);
					Log::info("FPS: {:.1f} UPS: {:.1f}", double(fps_counter) / elapsed, double(ups_counter) / elapsed);
					fps_counter = 0;
					time_point_counter = high_resolution_clock::now();
				}
			}

			auto last_state_ptr = world.getLastState();
			const voxen::WorldState &last_state = *last_state_ptr;
			// Input handle
			wnd.pollEvents();
			gui->update(last_state, render_to_world_queue);
			// GUI now handled a lot of callbacks (events) from Window
			// and update world via queue

			// Do render
			render->drawFrame(last_state, gui->view());
			fps_counter++;
		}

		g_stop.store(true);
		world_thread.join();

		// `render` and `gui` must be destroyed before calling `wnd.stop()`
		// TODO: do something about Window's lifetime management?
		render.reset();
		gui.reset();
		wnd.stop();
		voxen::ThreadPool::releaseGlobalVoxenPool();
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
