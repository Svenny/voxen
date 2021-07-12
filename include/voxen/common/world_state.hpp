#pragma once

#include <voxen/common/player.hpp>
#include <voxen/common/terrain/control_block.hpp>

#include <extras/function_ref.hpp>
#include <extras/refcnt_ptr.hpp>

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
	~WorldState() = default;

	Player &player() noexcept { return m_player; }
	const Player &player() const noexcept { return m_player; }

	void setRootControlBlock(extras::refcnt_ptr<terrain::ChunkControlBlock> ptr) noexcept { m_root_cb = std::move(ptr); }

	uint64_t tickId() const noexcept { return m_tick_id; }
	void setTickId(uint64_t value) noexcept { m_tick_id = value; }

	void walkActiveChunks(extras::function_ref<void(const terrain::Chunk &)> visitor) const;

private:
	Player m_player;
	extras::refcnt_ptr<terrain::ChunkControlBlock> m_root_cb;
	uint64_t m_tick_id = 0;
};

}
