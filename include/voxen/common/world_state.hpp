#pragma once

#include <voxen/common/player.hpp>
#include <voxen/common/terrain.hpp>

#include <functional>
#include <mutex>

namespace voxen
{

//TODO actual real queue
struct DebugQueueRtW {
	std::mutex mutex;
	glm::dvec3 player_forward_movement_direction{0.0f};
	glm::dvec3 player_strafe_movement_direction{0.0f};
	glm::dquat player_orientation = glm::identity<glm::dquat>();
	double strafe_speed{50};
	double forward_speed{25};
};

class WorldState {
public:
	WorldState() = default;
	WorldState(WorldState &&other) noexcept;
	WorldState(const WorldState &other);
	~WorldState() noexcept;

	Player &player() noexcept { return m_player; }
	const Player &player() const noexcept { return m_player; }

	TerrainOctree &terrain() { return *m_terrain; }
	const TerrainOctree &terrain() const { return *m_terrain; }
	void setTerrain(std::unique_ptr<TerrainOctree> &&ptr) noexcept { m_terrain = std::move(ptr); }

	uint64_t tickId() const noexcept { return m_tick_id; }
	void setTickId(uint64_t value) noexcept { m_tick_id = value; }

	void walkActiveChunks(std::function<void(const TerrainChunk &)> visitor) const;
private:
	Player m_player;
	std::unique_ptr<TerrainOctree> m_terrain;
	uint64_t m_tick_id = 0;
};

}
