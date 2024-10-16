#pragma once

#include <voxen/common/player.hpp>

#include <extras/function_ref.hpp>
#include <extras/refcnt_ptr.hpp>

#include <mutex>
#include <vector>

namespace voxen
{

namespace terrain
{
class Chunk;
}

//TODO actual real queue
struct DebugQueueRtW {
	std::mutex mutex;
	glm::dvec3 player_forward_movement_direction { 0.0 };
	glm::dvec3 player_strafe_movement_direction { 0.0 };
	glm::dquat player_orientation = glm::identity<glm::dquat>();
	double strafe_speed { 50 };
	double forward_speed { 25 };
	bool lock_chunk_loading_position;
};

class WorldState {
public:
	using ChunkPtrVector = std::vector<extras::refcnt_ptr<terrain::Chunk>>;
	using ChunkVisitor = extras::function_ref<void(const terrain::Chunk &)>;
	using ChunkPtrVisitor = extras::function_ref<void(const extras::refcnt_ptr<terrain::Chunk> &)>;

	WorldState() = default;
	WorldState(WorldState &&other) noexcept;
	WorldState(const WorldState &other);
	WorldState &operator=(WorldState &&) = delete;
	WorldState &operator=(const WorldState &) = delete;
	~WorldState() = default;

	Player &player() noexcept { return m_player; }
	const Player &player() const noexcept { return m_player; }

	void setActiveChunks(ChunkPtrVector value) noexcept { m_active_chunks = std::move(value); }

	uint64_t tickId() const noexcept { return m_tick_id; }
	void setTickId(uint64_t value) noexcept { m_tick_id = value; }

	void walkActiveChunks(ChunkVisitor visitor) const;
	// Same as `walkActiveChunks()`, but callback takes refcounted pointer to chunk instead of a reference.
	// This slightly increases risk of accidental copy, but allows to hold a chunk for more than one frame.
	void walkActiveChunksPointers(ChunkPtrVisitor visitor) const;

private:
	Player m_player;
	ChunkPtrVector m_active_chunks;
	uint64_t m_tick_id = 0;
};

} // namespace voxen
