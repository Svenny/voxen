#include <voxen/common/world_state.hpp>

#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

namespace voxen
{

WorldState::WorldState(WorldState &&other) noexcept
	: m_player(std::move(other.m_player)), m_terrain(std::move(other.m_terrain)), m_tick_id(other.m_tick_id)
{
}

WorldState::WorldState(const WorldState &other)
	: m_player(other.m_player), m_terrain(new TerrainOctree(*other.m_terrain)), m_tick_id(other.m_tick_id)
{
}

void WorldState::walkActiveChunks(std::function<void(const terrain::Chunk &)> visitor) const
{
	m_terrain->walkActiveChunks(std::move(visitor));
}

}
