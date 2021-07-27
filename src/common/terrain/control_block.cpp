#include <voxen/common/terrain/control_block.hpp>

#include <voxen/config.hpp>
#include <voxen/common/terrain/allocator.hpp>

#include <cassert>

namespace voxen::terrain
{

void ChunkControlBlock::clearTemporaryFlags() noexcept
{
	m_chunk_copied = false;
	m_chunk_changed = false;
	m_induced_seam_dirty = false;
	m_surface_builder.reset();
}

void ChunkControlBlock::copyChunk()
{
	assert(m_chunk);

	if (m_chunk_copied) {
		return;
	}

	const Chunk *old_chunk = m_chunk.get();
	m_chunk = PoolAllocator::allocateChunk(Chunk::CreationInfo {
		.id = old_chunk->id(),
		.version = old_chunk->version(),
		.reuse_type = Chunk::ReuseType::NoSeam,
		.reuse_chunk = old_chunk
	});

	m_chunk_copied = true;
	m_surface_builder.reset();
}

void ChunkControlBlock::setChunk(extras::refcnt_ptr<Chunk> ptr)
{
	m_chunk = std::move(ptr);
	m_surface_builder.reset();
}

void ChunkControlBlock::validateState(bool has_active_parent, bool can_seam_dirty, bool can_chunk_changed) const
{
	if constexpr (true || BuildConfig::kIsReleaseBuild) {
		(void) has_active_parent;
		(void) can_seam_dirty;
		(void) can_chunk_changed;
		// This code consists only of asserts, hothing to do here in release
		return;
	}

	// "Seam dirty" flag must propagate up to the root, i.e.
	// there must be no "dirty" chunks with "clean" parent
	assert(can_seam_dirty || !m_induced_seam_dirty);
	// Same with "chunk changed" flag
	assert(can_chunk_changed || !m_chunk_changed);

	if (m_chunk_changed) {
		assert(m_induced_seam_dirty);
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
			child->validateState(is_active || has_active_parent, m_induced_seam_dirty, m_chunk_changed);
			has_children = true;
		}
	}

	if (!has_children) {
		// Any path to leaf must go through an active chunk
		assert(is_active || has_active_parent);
	}
}

}
