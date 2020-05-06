#include <voxen/common/world.hpp>
#include <voxen/util/log.hpp>

#include <glm/ext/quaternion_exponential.hpp>

namespace voxen
{

World::World() : m_terrain(/*1<<21*/16, /*1 << 12*/4) {
}

World::World(const World &other) : m_player(other.m_player), m_terrain(other.m_terrain) {
}

World::~World() {}

void World::update(DebugQueueRtW& queue, std::chrono::duration<int64_t, std::nano> tick_inverval) {
	// Update user position
	double dt = std::chrono::duration_cast<std::chrono::duration<double>>(tick_inverval).count();
	glm::dvec3 pos = m_player.position();
	pos += queue.player_forward_movement_direction * (queue.forward_speed * dt);
	pos += queue.player_strafe_movement_direction * (queue.strafe_speed * dt);
	glm::dquat rot = m_player.orientation();
	//rot = glm::normalize(rot * glm::pow(queue.player_rotation_quat, dt));
	rot = glm::normalize(queue.player_rotation_quat * rot);
	m_player.updateState(pos, rot);

	m_terrain.updateChunks(pos.x, pos.y, pos.z);
}

void World::walkActiveChunks(std::function<void (const TerrainChunk &)> visitor) const {
	m_terrain.walkActiveChunks(visitor);
}

void World::render(VulkanRender &render) {
	m_terrain.walkActiveChunks([&](const TerrainChunk &chunk) {
		float x = float(chunk.baseX());
		float y = float(chunk.baseY());
		float z = float(chunk.baseZ());
		float sz = float(chunk.size() * chunk.scale());
		render.debugDrawOctreeNode(m_player, x, y, z, sz);
	});
}

}
