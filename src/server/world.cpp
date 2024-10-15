#include <voxen/server/world.hpp>

#include <voxen/util/log.hpp>

namespace voxen::server
{

World::World()
{
	Log::debug("Creating server World");

	auto initial_state_ptr = std::make_shared<WorldState>();
	initial_state_ptr->setActiveChunks(m_terrain_controller.doTick());
	m_last_state_ptr.store(std::move(initial_state_ptr), std::memory_order_release);

	Log::debug("Server World created successfully");
}

World::~World() noexcept
{
	Log::debug("Destroying server World");
}

std::shared_ptr<const WorldState> World::getLastState() const
{
	return m_last_state_ptr.load(std::memory_order_acquire);
}

void World::update(DebugQueueRtW &queue, std::chrono::duration<int64_t, std::nano> tick_inverval)
{
	auto last_state_ptr = getLastState();
	const WorldState &last_state = *last_state_ptr;

	auto next_state_ptr = std::make_shared<WorldState>(last_state);
	WorldState &next_state = *next_state_ptr;

	next_state.setTickId(last_state.tickId() + 1);

	// Update user position
	double dt = std::chrono::duration_cast<std::chrono::duration<double>>(tick_inverval).count();
	auto &player = next_state.player();
	glm::dvec3 pos = player.position();
	{
		std::lock_guard<std::mutex> lock { queue.mutex };
		pos += queue.player_forward_movement_direction * (queue.forward_speed * dt);
		pos += queue.player_strafe_movement_direction * (queue.strafe_speed * dt);
		player.updateState(pos, queue.player_orientation);
	}

	// Update chunks
	if (!queue.lock_chunk_loading_position) {
		m_chunk_loading_position = pos;
	}

	m_terrain_controller.setPointOfInterest(0, m_chunk_loading_position);
	next_state.setActiveChunks(m_terrain_controller.doTick());

	m_last_state_ptr.store(std::move(next_state_ptr), std::memory_order_release);
}

} // namespace voxen::server
