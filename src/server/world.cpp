#include <voxen/server/world.hpp>

#include <voxen/common/player_state_message.hpp>
#include <voxen/svc/messaging_service.hpp>
#include <voxen/svc/service_locator.hpp>
#include <voxen/util/log.hpp>

namespace voxen::server
{

World::World(svc::ServiceLocator &svc) : m_terrain_controller(svc)
{
	Log::debug("Creating server World");

	m_message_queue = svc.requestService<svc::MessagingService>().registerAgent(SERVICE_UID);
	m_message_queue.registerHandler<PlayerStateMessage>(
		[this](PlayerStateMessage &msg, svc::MessageInfo &info) { handlePlayerInputMessage(msg, info); });

	auto initial_state_ptr = std::make_shared<WorldState>();
	initial_state_ptr->setActiveChunks(m_terrain_controller.doTick());
	m_last_state_ptr.store(std::move(initial_state_ptr), std::memory_order_release);

	m_world_thread = std::thread(worldThreadProc, std::ref(*this));

	Log::debug("Server World created successfully");
}

World::~World() noexcept
{
	Log::debug("Destroying server World");
	m_thread_stop.store(true);
	m_world_thread.join();
}

std::shared_ptr<const WorldState> World::getLastState() const
{
	return m_last_state_ptr.load(std::memory_order_acquire);
}

void World::update()
{
	auto last_state_ptr = getLastState();
	const WorldState &last_state = *last_state_ptr;

	auto next_state_ptr = std::make_shared<WorldState>(last_state);
	WorldState &next_state = *next_state_ptr;

	next_state.setTickId(last_state.tickId() + 1);

	// Receive player input messages
	m_next_state = &next_state;
	m_message_queue.pollMessages();
	m_next_state = nullptr;

	// Update chunks
	m_terrain_controller.setPointOfInterest(0, m_chunk_loading_position);
	next_state.setActiveChunks(m_terrain_controller.doTick());

	m_last_state_ptr.store(std::move(next_state_ptr), std::memory_order_release);
}

void World::handlePlayerInputMessage(PlayerStateMessage &msg, svc::MessageInfo & /*info*/) noexcept
{
	assert(m_next_state != nullptr);

	m_next_state->player().updateState(msg.player_position, msg.player_orientation);

	if (!msg.lock_chunk_loading_position) {
		m_chunk_loading_position = msg.player_position;
	}
}

void World::worldThreadProc(World &me)
{
	auto last_tick_time = std::chrono::steady_clock::now();
	const std::chrono::duration<int64_t, std::nano> tick_inverval { int64_t(me.secondsPerTick() * 1'000'000'000.0) };
	auto next_tick_time = last_tick_time + tick_inverval;

	while (!me.m_thread_stop.load()) {
		auto cur_time = std::chrono::steady_clock::now();

		while (cur_time >= next_tick_time) {
			me.update();
			next_tick_time += tick_inverval;

			if (me.m_thread_stop.load()) {
				// Don't needlessly sleep if we are ordered to stop
				break;
			}
		}

		std::this_thread::sleep_until(next_tick_time - std::chrono::milliseconds(1));
	}
}

} // namespace voxen::server
