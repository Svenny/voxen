#include <voxen/common/terrain/control_block.hpp>

#include <voxen/config.hpp>
#include <voxen/common/terrain/allocator.hpp>

#include <cassert>

namespace voxen::terrain
{

void ChunkControlBlock::setChunk(extras::refcnt_ptr<Chunk> ptr)
{
	m_chunk = std::move(ptr);
}

void ChunkControlBlock::validateState(bool has_active_parent, bool can_chunk_changed) const
{
	if (!can_chunk_changed) {
		assert(!m_chunk_changed);
	}

	const bool is_active = m_state == ChunkControlBlock::State::Active;

	if (is_active) {
		// Active chunk must not be marked as "over active"
		assert(!m_over_active);
		// Active chunk must not have another active parent
		assert(!has_active_parent);
	} else if (!has_active_parent) {
		// No active parent means this chunk is parent of some active one

		// Any parent of active chunk must be marked as "over active"
		assert(m_over_active);
		// Active chunk must not have any invalid, loading or active parent
		assert(m_state == ChunkControlBlock::State::Standby);
	} else {
		// If chunk is marked as "over active" it must not have an active parent
		assert(!m_over_active);
	}

	bool has_children = false;
	for (const auto &child : m_children) {
		if (child) {
			child->validateState(is_active || has_active_parent, m_chunk_changed);
			has_children = true;
		}
	}

	if (!has_children) {
		// Any path to leaf must go through an active chunk
		assert(is_active || has_active_parent);
	}
}

}
