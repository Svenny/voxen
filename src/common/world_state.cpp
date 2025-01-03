#include <voxen/common/terrain/chunk.hpp>
#include <voxen/common/world_state.hpp>

#include <cassert>

namespace voxen
{

WorldState::WorldState(WorldState &&other) noexcept
	: m_player(std::move(other.m_player))
	, m_active_chunks(std::move(other.m_active_chunks))
	, m_land_state(std::move(other.m_land_state))
	, m_tick_id(other.m_tick_id)
{}

WorldState::WorldState(const WorldState &other)
	: m_player(other.m_player)
	, m_active_chunks(other.m_active_chunks)
	, m_land_state(other.m_land_state)
	, m_tick_id(other.m_tick_id)
{}

void WorldState::walkActiveChunks(ChunkVisitor visitor) const
{
	for (const auto &ptr : m_active_chunks) {
		visitor(*ptr);
	}
}

void WorldState::walkActiveChunksPointers(ChunkPtrVisitor visitor) const
{
	for (const auto &ptr : m_active_chunks) {
		visitor(ptr);
	}
}

} // namespace voxen
