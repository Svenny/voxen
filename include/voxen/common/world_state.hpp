#pragma once

#include <voxen/common/player.hpp>
#include <voxen/util/tagged_tick_id.hpp>

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

struct WorldTickTag {};

using WorldTickId = TaggedTickId<WorldTickTag>;

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

	WorldTickId tickId() const noexcept { return m_tick_id; }
	void setTickId(WorldTickId value) noexcept { m_tick_id = value; }

	void walkActiveChunks(ChunkVisitor visitor) const;
	// Same as `walkActiveChunks()`, but callback takes refcounted pointer to chunk instead of a reference.
	// This slightly increases risk of accidental copy, but allows to hold a chunk for more than one frame.
	void walkActiveChunksPointers(ChunkPtrVisitor visitor) const;

private:
	Player m_player;
	ChunkPtrVector m_active_chunks;
	WorldTickId m_tick_id { 0 };
};

} // namespace voxen
