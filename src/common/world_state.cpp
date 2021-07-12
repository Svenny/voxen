#include <voxen/common/world_state.hpp>

#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

#include <cassert>
#include <vector>

namespace voxen
{

WorldState::WorldState(WorldState &&other) noexcept
	: m_player(std::move(other.m_player)), m_tick_id(other.m_tick_id)
{
}

WorldState::WorldState(const WorldState &other)
	: m_player(other.m_player), m_tick_id(other.m_tick_id)
{
}

void WorldState::walkActiveChunks(extras::function_ref<void(const terrain::Chunk &)> visitor) const
{
	if (!m_root_cb) {
		return;
	}

	std::vector<const terrain::ChunkControlBlock *> stack;
	stack.reserve(20);

	stack.emplace_back(m_root_cb.get());
	while (!stack.empty()) {
		const terrain::ChunkControlBlock *cb = stack.back();
		stack.pop_back();

		if (cb->state() == terrain::ChunkControlBlock::State::Active) {
			assert(cb->chunk());
			visitor(*cb->chunk());
		}

		for (unsigned i = 0; i < 8; i++) {
			if (cb->child(i)) {
				stack.emplace_back(cb->child(i));
			}
		}
	}
}

}
