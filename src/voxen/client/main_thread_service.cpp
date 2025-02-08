#include <voxen/client/main_thread_service.hpp>

#include <voxen/client/gui.hpp>
#include <voxen/client/render.hpp>
#include <voxen/common/config.hpp>
#include <voxen/debug/thread_name.hpp>
#include <voxen/gfx/ui/ui_builder.hpp>
#include <voxen/os/glfw_window.hpp>
#include <voxen/svc/messaging_service.hpp>
#include <voxen/svc/service_locator.hpp>
#include <voxen/util/log.hpp>
#include <voxen/world/world_control_service.hpp>
#include <voxen/world/world_state.hpp>

#include <extras/attributes.hpp>

#include <chrono>

namespace voxen::client
{

namespace
{

struct GlfwRaii {
	GlfwRaii() { os::GlfwWindow::initGlfw(); }
	~GlfwRaii() { os::GlfwWindow::terminateGlfw(); }
};

} // namespace

struct MainThreadService::Impl {
	svc::ServiceLocator &svc;
	Config cfg;

	bool log_fps = false;

	// Used for sending player state to World
	svc::MessageSender message_sender;

	// Placed before GLFW-dependent stuff to construct before it and destroy after it
	[[EXTRAS_NO_UNIQUE_ADDRESS]] GlfwRaii glfw_raii;

	os::GlfwWindow window;
	// Placed after `window` to destroy before it
	std::unique_ptr<Render> render_service;
	// Placed after `window` to destroy before it
	std::unique_ptr<Gui> gui;
};

MainThreadService::MainThreadService(svc::ServiceLocator &svc, Config cfg) : m_impl(svc, cfg)
{
	auto &impl = m_impl.object();

	impl.message_sender = svc.requestService<svc::MessagingService>().createSender(SERVICE_UID);

	auto *main_config = voxen::Config::mainConfig();

	impl.log_fps = main_config->getBool("dev", "fps_logging");

	m_impl->window = os::GlfwWindow({
		.width = main_config->getInt32("window", "width"),
		.height = main_config->getInt32("window", "height"),
		.title = "Voxen",
		.fullscreen = main_config->getBool("window", "fullscreen"),
	});

	m_impl->render_service = std::make_unique<Render>(m_impl->window, svc);
	m_impl->gui = std::make_unique<Gui>(m_impl->window);
}

MainThreadService::~MainThreadService() noexcept = default;

void MainThreadService::doMainLoop(FrameCallback frame_callback)
{
	debug::setThreadName("Main Thread");

	auto &impl = m_impl.object();

	auto &world_control = impl.svc.requestService<world::ControlService>();
	auto last_state_ptr = world_control.getLastState();

	impl.gui->init(*last_state_ptr);

	int64_t fps_counter = 0;
	world::TickId tick_id_counter = last_state_ptr->tickId();

	auto last_fps_log_time = std::chrono::steady_clock::now();
	auto last_input_sample_time = last_fps_log_time;

	while (!impl.window.shouldClose()) {
		// Write all possibly buffered log messages
		fflush(stdout);

		// Record time when we started receiving input events (input sampling time)
		auto input_sample_time = std::chrono::steady_clock::now();

		// Receive input events
		impl.window.pollEvents();

		// Receive the latest world state
		last_state_ptr = world_control.getLastState();
		const world::State &last_state = *last_state_ptr;

		if (impl.log_fps) {
			std::chrono::duration<double> dur = (input_sample_time - last_fps_log_time);
			double elapsed = dur.count();
			if (elapsed > 2.0) {
				world::TickId tick_id = last_state.tickId();
				int64_t ups_counter = tick_id - tick_id_counter;

				Log::info("FPS: {:.1f} UPS: {:.1f}", double(fps_counter) / elapsed, double(ups_counter) / elapsed);

				fps_counter = 0;
				tick_id_counter = tick_id;
				last_fps_log_time = input_sample_time;
			}
		}

		// Count time delta (in seconds)
		std::chrono::duration<double> input_sample_duration = input_sample_time - last_input_sample_time;
		double dt = input_sample_duration.count();
		// Don't forget to store current sample time as the last one
		last_input_sample_time = input_sample_time;

		// Convert sampled input events into actions (player controls)
		// TODO: this is not our responsibility, user code should do it
		impl.gui->update(last_state, dt, impl.message_sender);

		gfx::ui::UiBuilder ui_bld;

		FrameCallbackData fcd {
			.delta_time = dt,
			.ui_builder = ui_bld,
		};

		// Perform per-frame user logic
		if (!frame_callback(fcd)) {
			// Requested to stop
			break;
		}

		// TODO: use true window dimensions
		// TODO: use the result (actually draw UI), this call is just for debugging
		ui_bld.computeLayout(2560, 1440);

		// Do render
		impl.render_service->drawFrame(last_state, impl.gui->view());
		fps_counter++;
	}
}

} // namespace voxen::client
