#include <voxen/common/terrain/control_block.hpp>

#include <voxen/config.hpp>
#include <voxen/common/terrain/allocator.hpp>
#include <voxen/util/log.hpp>

#include <cassert>
#include <map>

namespace voxen::terrain
{

ChunkControlBlock::ChunkControlBlock(CreationInfo info)
{
	const ChunkControlBlock *pred = info.predecessor;

	if (pred) {
		m_state = pred->m_state;
		m_seam_dirty = pred->m_seam_dirty;
		m_over_active = pred->m_over_active;

		if (pred->m_chunk) {
			const Chunk *base = pred->m_chunk.get();

			if (info.reset_seam) {
				m_chunk = PoolAllocator::allocateChunk(Chunk::CreationInfo {
					.id = base->id(),
					.version = base->version(),
					.reuse_type = Chunk::ReuseType::NoSeam,
					.reuse_chunk = base
				});
			} else {
				m_chunk = pred->chunkPtr();
			}

			m_surface_builder = std::make_unique<SurfaceBuilder>(*m_chunk);
		}

		for (unsigned i = 0; i < 8; i++) {
			m_children[i] = pred->m_children[i];
		}
	}
}

ChunkControlBlock::~ChunkControlBlock() noexcept// = default;
{
	// TODO: temporary for debugging, to certainly break on use-after-free
	m_state = State::Invalid;
	m_chunk.reset();
	for (auto &ptr : m_children) {
		ptr.reset();
	}
}

void ChunkControlBlock::setChunk(extras::refcnt_ptr<Chunk> ptr)
{
	m_chunk = std::move(ptr);
	m_surface_builder = std::make_unique<SurfaceBuilder>(*m_chunk);
}

void ChunkControlBlock::validateState(bool has_active_parent, bool can_seam_dirty) const
{
	if constexpr (BuildConfig::kIsReleaseBuild) {
		(void) has_active_parent;
		(void) can_seam_dirty;
		// This code consists only of asserts, hothing to do here in release
		return;
	}

	// "Seam dirty" flag must propagate up to the root, i.e.
	// there must be no "dirty" chunks with "clean" parent
	assert(can_seam_dirty || !m_seam_dirty);

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
			child->validateState(is_active || has_active_parent, m_seam_dirty);
			has_children = true;
		}
	}

	if (!has_children) {
		// Any path to leaf must go through an active chunk
		assert(is_active || has_active_parent);
	}
}

void ChunkControlBlock::printStats() const
{
	if constexpr (BuildConfig::kIsReleaseBuild) {
		// This code is too heavyweight to run in release
		return;
	}

	if (!Log::willBeLogged(Log::Level::Trace)) {
		return;
	}

	std::unordered_map<ChunkControlBlock::State, size_t> state_counts;
	std::map<uint32_t, size_t> lod_actives;
	std::map<uint32_t, size_t> lod_dirty_actives;
	std::map<uint32_t, size_t> lod_dirty_inactives;

	std::vector<const ChunkControlBlock *> stack;
	stack.emplace_back(this);

	while (!stack.empty()) {
		const ChunkControlBlock *cb = stack.back();
		stack.pop_back();
		if (!cb) {
			continue;
		}

		state_counts[cb->state()]++;

		if (cb->chunk()) {
			uint32_t lod = cb->chunk()->id().lod;
			if (cb->state() == ChunkControlBlock::State::Active) {
				lod_actives[lod]++;
				if (cb->isSeamDirty()) {
					lod_dirty_actives[lod]++;
				}
			} else if (cb->isSeamDirty()) {
				lod_dirty_inactives[lod]++;
			}
		}

		for (int i = 0; i < 8; i++) {
			stack.emplace_back(cb->child(i));
		}
	}

	Log::trace("Dirty stats: loading {}, standby {}, active {}", state_counts[ChunkControlBlock::State::Loading],
	           state_counts[ChunkControlBlock::State::Standby], state_counts[ChunkControlBlock::State::Active]);
	const uint32_t max_lod = m_chunk->id().lod;
	for (uint32_t lod = 0; lod <= max_lod; lod++) {
		Log::trace("{} => active {}, dirty active {}, dirty inactive {}", lod, lod_actives[lod],
		           lod_dirty_actives[lod], lod_dirty_inactives[lod]);
	}
}

}
