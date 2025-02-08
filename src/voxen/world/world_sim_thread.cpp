#include "world_sim_thread.hpp"

#include <voxen/common/player_state_message.hpp>
#include <voxen/debug/thread_name.hpp>
#include <voxen/land/land_service.hpp>
#include <voxen/svc/messaging_service.hpp>
#include <voxen/svc/service_locator.hpp>
#include <voxen/svc/task_builder.hpp>
#include <voxen/svc/task_coro.hpp>
#include <voxen/svc/task_service.hpp>
#include <voxen/util/log.hpp>
#include <voxen/world/world_control_service.hpp>

namespace voxen::world::detail
{

namespace
{

svc::CoroTask saveWorldTask(std::shared_ptr<const State> state, ControlService::SaveRequest req)
{
	Log::warn("TODO: world save/load is not yet implemented, discarding save request");
	(void) state;

	if (req.progress_callback) {
		// TODO: just for debugging callbacks, simulate saving delay
		constexpr int STEPS = 5;
		for (int i = 0; i <= STEPS; i++) {
			req.progress_callback(static_cast<float>(i) / static_cast<float>(STEPS));
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
	}

	if (req.result_callback) {
		// Saved kinda successfully (nothing to fail here really)
		req.result_callback(std::error_condition {});
	}

	co_return;
}

} // namespace

SimThread::SimThread(Private, svc::ServiceLocator &svc) : m_terrain_controller(svc)
{
	m_land_service = &svc.requestService<land::LandService>();
	m_task_service = &svc.requestService<svc::TaskService>();

	m_message_queue = svc.requestService<svc::MessagingService>().registerAgent(ControlService::SERVICE_UID);
	m_message_queue.registerHandler<PlayerStateMessage>(
		[this](PlayerStateMessage &msg, svc::MessageInfo &info) { handlePlayerInputMessage(msg, info); });

	// Initialize with empty state, tick zero
	m_last_state_ptr.store(std::make_shared<State>(), std::memory_order_release);
}

std::shared_ptr<SimThread> SimThread::create(svc::ServiceLocator &svc, ControlService::StartRequest start_req)
{
	auto ptr = std::make_shared<SimThread>(Private {}, svc);

	// Start a thread and detach it immediately, it must be stopped with a stop command
	std::thread(worldThreadProc, ptr, std::move(start_req)).detach();

	return ptr;
}

void SimThread::requestSave(ControlService::SaveRequest req)
{
	std::lock_guard lock(m_cmd_queue_lock);
	m_cmd_queue.push(SaveCommand { std::move(req) });
}

void SimThread::requestStop(ControlService::SaveRequest req)
{
	std::lock_guard lock(m_cmd_queue_lock);
	m_cmd_queue.push(StopCommand { std::move(req) });
}

void SimThread::update()
{
	auto last_state_ptr = getLastState();
	const State &last_state = *last_state_ptr;

	auto next_state_ptr = std::make_shared<State>(last_state);
	State &next_state = *next_state_ptr;

	next_state.setTickId(last_state.tickId() + 1);

	// Receive player input messages
	m_next_state = &next_state;
	m_message_queue.pollMessages();
	m_next_state = nullptr;

#if 1
	static glm::dvec3 s_prev_pos = last_state.player().position();
	static std::chrono::steady_clock::time_point s_prev_time = std::chrono::steady_clock::now();
	if (next_state.tickId().value % 750 == 0) {
		glm::dvec3 now_pos = next_state.player().position();
		auto now_time = std::chrono::steady_clock::now();

		double distance = glm::distance(s_prev_pos, now_pos);
		double speed = distance / std::chrono::duration<double>(now_time - s_prev_time).count();
		if (distance > 0.0) {
			Log::info("Velocity {} m/s; position: {} {} {}", speed, now_pos.x, now_pos.y, now_pos.z);
		}

		s_prev_pos = now_pos;
		s_prev_time = now_time;
	}
#endif

	m_land_service->doTick(next_state.tickId());
	next_state.setLandState(m_land_service->stateForCopy());

	m_last_state_ptr.store(std::move(next_state_ptr), std::memory_order_release);
}

void SimThread::handlePlayerInputMessage(PlayerStateMessage &msg, svc::MessageInfo & /*info*/) noexcept
{
	assert(m_next_state != nullptr);

	m_next_state->player().updateState(msg.player_position, msg.player_orientation);

	if (!msg.lock_chunk_loading_position) {
		m_chunk_loading_position = msg.player_position;
	}
}

void SimThread::worldThreadProc(std::shared_ptr<SimThread> me, ControlService::StartRequest start_request)
{
	debug::setThreadName("WorldSimThread");
	Log::info("World sim thread started");

	if (!start_request.storage_directory.empty()) {
		Log::warn("TODO: world save/load is not yet implemented, generating a new one");
	}

	if (start_request.progress_callback) {
		// TODO: just for debugging callbacks, simulate loading delay
		constexpr int STEPS = 5;
		for (int i = 0; i <= STEPS; i++) {
			start_request.progress_callback(static_cast<float>(i) / static_cast<float>(STEPS));
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
	}

	if (start_request.result_callback) {
		// Started successfully (nothing to fail here really)
		start_request.result_callback(std::error_condition {});
	}

	auto last_tick_time = std::chrono::steady_clock::now();
	const std::chrono::duration<int64_t, std::nano> tick_inverval { int64_t(SECONDS_PER_TICK * 1'000'000'000.0) };
	auto next_tick_time = last_tick_time + tick_inverval;

	bool stop = false;

	auto process_commands = [&]() {
		std::unique_lock lock(me->m_cmd_queue_lock);

		while (!me->m_cmd_queue.empty()) {
			auto cmd = std::move(me->m_cmd_queue.front());
			me->m_cmd_queue.pop();

			// Release lock to allow pushing more commands
			lock.unlock();

			if (auto *save_cmd = std::get_if<SaveCommand>(&cmd); save_cmd) {
				svc::TaskBuilder bld(*me->m_task_service);
				bld.enqueueTask(saveWorldTask(me->getLastState(), std::move(save_cmd->request)));
			} else if (auto *stop_cmd = std::get_if<StopCommand>(&cmd); stop_cmd) {
				stop = true;
				// We won't execute any more updates after raising stop flag
				svc::TaskBuilder bld(*me->m_task_service);
				bld.enqueueTask(saveWorldTask(me->getLastState(), std::move(stop_cmd->request)));
			} else {
				assert(false);
				Log::error("Unknown command to world sim thread, dropping");
			}

			// Acquire lock again before checking the queue again
			lock.lock();
		}
	};

	while (!stop) {
		auto cur_time = std::chrono::steady_clock::now();

		while (cur_time >= next_tick_time) {
			me->update();
			next_tick_time += tick_inverval;

			process_commands();

			if (stop) {
				// Don't needlessly sleep if we are ordered to stop
				break;
			}
		}

		std::this_thread::sleep_until(next_tick_time);
	}

	Log::info("World sim thread stopped");
}

} // namespace voxen::world::detail
