#pragma once

#include <voxen/common/player_state_message.hpp>
#include <voxen/common/terrain/controller.hpp>
#include <voxen/land/land_fwd.hpp>
#include <voxen/os/futex.hpp>
#include <voxen/svc/message_queue.hpp>
#include <voxen/svc/service_base.hpp>
#include <voxen/visibility.hpp>
#include <voxen/world/world_control_service.hpp>
#include <voxen/world/world_state.hpp>

#include <atomic>
#include <memory>

namespace voxen::world::detail
{

class SimThread {
private:
	// Hack to make `SimThread` ctor accessible only from within `create()`.
	// TODO: refactor into a common "passkey" utility.
	struct Private {
		explicit Private() = default;
	};

public:
	constexpr static double SECONDS_PER_TICK = 1.0 / 100.0; // 100 UPS

	SimThread(Private, svc::ServiceLocator &svc);
	SimThread(SimThread &&) = delete;
	SimThread(const SimThread &) = delete;
	SimThread &operator=(SimThread &&) = delete;
	SimThread &operator=(const SimThread &) = delete;
	~SimThread() = default;

	static std::shared_ptr<SimThread> create(svc::ServiceLocator &svc, ControlService::StartRequest start_req);

	void requestSave(ControlService::SaveRequest req);
	void requestStop(ControlService::SaveRequest req);

	std::shared_ptr<const State> getLastState() const { return m_last_state_ptr.load(std::memory_order_acquire); }

private:
	struct SaveCommand {
		ControlService::SaveRequest request;
	};

	struct StopCommand {
		ControlService::SaveRequest request;
	};

	terrain::Controller m_terrain_controller;
	land::LandService *m_land_service = nullptr;
	svc::TaskService *m_task_service = nullptr;

	// `getLastState()` and `update()` may be called from different
	// threads simultaneously. Therefore this pointer is atomic.
	std::atomic<std::shared_ptr<State>> m_last_state_ptr;

	glm::dvec3 m_chunk_loading_position = {};
	State *m_next_state = nullptr;

	svc::MessageQueue m_message_queue;

	os::FutexLock m_cmd_queue_lock;
	std::queue<std::variant<SaveCommand, StopCommand>> m_cmd_queue;

	void update();

	void handlePlayerInputMessage(PlayerStateMessage &msg, svc::MessageInfo &info) noexcept;

	static void worldThreadProc(std::shared_ptr<SimThread> me, ControlService::StartRequest start_request);
};

} // namespace voxen::world::detail
