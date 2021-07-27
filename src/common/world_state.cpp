#include <voxen/common/world_state.hpp>
#include <voxen/common/terrain/chunk.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

#include <cassert>
#include <vector>

namespace voxen
{

WorldState::WorldState(WorldState &&other) noexcept
	: m_player(std::move(other.m_player)), m_active_chunks(std::move(other.m_active_chunks)), m_tick_id(other.m_tick_id)
{
}

WorldState::WorldState(const WorldState &other)
	: m_player(other.m_player), m_active_chunks(other.m_active_chunks), m_tick_id(other.m_tick_id)
{
}

void WorldState::walkActiveChunks(extras::function_ref<void(const terrain::Chunk &)> visitor) const
{
	for (const auto &ptr : m_active_chunks) {
		visitor(*ptr);
	}
}

}
