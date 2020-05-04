#pragma once

#include <chrono>
#include <voxen/common/player.hpp>
#include <voxen/common/terrain.hpp>

#include <voxen/client/vulkan/vulkan_render.hpp>

#include <functional>

namespace voxen
{

//TODO actual real queue
struct DebugQueueRtW {
	glm::dvec3 player_forward_movement_direction{0.0f};
	glm::dvec3 player_strafe_movement_direction{0.0f};
	glm::dquat player_rotation_quat = glm::identity<glm::dquat>();
	double strafe_speed{50};
	double forward_speed{25};
};

class World {
public:
	World();
	World(const World &other);
	~World();

	const Player &player() const noexcept { return m_player; }

	double secondsPerTick() const noexcept { return 1.0 / 100.0; } // 100 UPS

	void update(DebugQueueRtW& queue, std::chrono::duration<int64_t, std::nano> tick_inverval);

	void walkActiveChunks(std::function<void(const TerrainChunk &)> visitor) const;
	void render(VulkanRender &render);
public:
	Player m_player;
	TerrainOctree m_terrain;
};

}
