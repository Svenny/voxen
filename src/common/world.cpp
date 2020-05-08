#include <voxen/common/world.hpp>
#include <voxen/util/log.hpp>

namespace voxen
{

World::World() : m_terrain(/*1<<21*/16, /*1 << 12*/4), m_tick_id(0) {
}

World::World(const World &other) : m_player(other.m_player), m_terrain(other.m_terrain), m_tick_id(other.m_tick_id) {
}

World::~World() {}

void World::update(DebugQueueRtW& queue, std::chrono::duration<int64_t, std::nano> tick_inverval) {
	m_tick_id++;

	// Update user position
	double dt = std::chrono::duration_cast<std::chrono::duration<double>>(tick_inverval).count();
	glm::dvec3 pos = m_player.position();
	pos += queue.player_forward_movement_direction * (queue.forward_speed * dt);
	pos += queue.player_strafe_movement_direction * (queue.strafe_speed * dt);
	m_player.updateState(pos, queue.player_orientation);

	// Update chunks
	m_terrain.updateChunks(pos.x, pos.y, pos.z);
}

void World::walkActiveChunks(std::function<void (const TerrainChunk &)> visitor) const {
	m_terrain.walkActiveChunks(visitor);
}

}
