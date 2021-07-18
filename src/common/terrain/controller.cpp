#include <voxen/common/terrain/controller.hpp>

#include <voxen/config.hpp>
#include <voxen/common/threadpool.hpp>
#include <voxen/common/terrain/allocator.hpp>
#include <voxen/common/terrain/chunk.hpp>
#include <voxen/common/terrain/control_block.hpp>
#include <voxen/common/terrain/coord.hpp>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>

namespace voxen::terrain
{

// Maximal time, in ticks, after which non-updated point of interest will be discared
// TODO (Svenny): move to `terrain/config.hpp`
constexpr static uint64_t MAX_POI_AGE = 1000;
// Maximal number of direct (non-seam) chunk ops which can be queued during one tick
// TODO (Svenny): move to `terrain/config.hpp`
constexpr static uint32_t MAX_DIRECT_OP_COUNT = 32;

Controller::Controller()
{
	// TODO: set it externally
	constexpr uint32_t MAX_LOD = 12u;

	// Load root chunk immediately to avoid dealing with "no active chunks" corner case
	m_root_cb = PoolAllocator::allocateControlBlock(ChunkControlBlock::CreationInfo {
		.predecessor = nullptr
	});
	m_root_cb->setChunk(m_loader.load(ChunkId {
		.lod = MAX_LOD, .base_x = 0, .base_y = 0, .base_z = 0
	}));
	m_root_cb->setState(ChunkControlBlock::State::Active);
	m_root_cb->setSeamDirty(true);
}

Controller::~Controller() noexcept = default;

Controller::ControlBlockPtr Controller::doTick()
{
	m_tick_id++;

	m_root_cb->validateState();
	garbageCollectPointsOfInterest();

	m_new_cbs.clear();
	m_load_quota = 0;

	OuterUpdateResult update_result = updateChunk(*m_root_cb);
	if (update_result.has_value()) {
		// Phase 1 - propagate "seam dirty" flags to seam-dependent chunks
		auto ptr = seamCellProcPhase1((*update_result).get());
		// If we have entered this `if` then root was already
		// COW-copied, it must not do this for a second time
		assert(!ptr.has_value() || !(*ptr));
		(*update_result)->printStats();
		// Phase 2 - rebuild seams and clear "seam dirty" flags
		seamCellProcPhase2((*update_result).get());

		m_root_cb = *std::move(update_result);
	}

	return m_root_cb;
}

void Controller::setPointOfInterest(uint64_t id, const glm::dvec3 &position)
{
	for (auto &point : m_points_of_interest) {
		if (point.id == id) {
			point.last_update_tick_id = m_tick_id;
			point.position = position;
			return;
		}
	}

	m_points_of_interest.emplace_back(PointOfInterest {
		.id = id,
		.last_update_tick_id = m_tick_id,
		.position = position
	});
}

uint32_t Controller::calcLodDirection(ChunkId id) const
{
	// This value multiplied by chunk side size gives an
	// average of inscribed and circumscribed spheres' radii
	constexpr double PSEUDORADIUS_MULT = (glm::root_two<double>() + 1.0) / 2.0;
	// The target angular diameter of a single chunk, LODs will be adjusted to reach it
	constexpr double OPTIMAL_PHI = glm::radians(50.0);
	const double OPTIMAL_TAN_HALF_PHI = glm::tan(OPTIMAL_PHI * 0.5);

	// Radius of a sphere used to approximate angular diameter of a chunk
	const double delta = double(Config::CHUNK_SIZE << id.lod) * PSEUDORADIUS_MULT;
	// Center of chunk in world-space coordinates
	const glm::dvec3 center = CoordUtils::chunkLocalToWorld(id, glm::dvec3(Config::CHUNK_SIZE >> 1u));

	uint32_t target_lod = UINT32_MAX;

	for (const auto &point : m_points_of_interest) {
		const double dist = glm::distance(center, point.position);
		if (dist <= delta) {
			// We are inside the chunk, it must be LOD 0 in this case
			return 0;
		}

		const double target_delta = dist * OPTIMAL_TAN_HALF_PHI;
		const double target_size = target_delta / (PSEUDORADIUS_MULT * double(Config::CHUNK_SIZE));
		target_lod = std::min(target_lod, uint32_t(glm::log2(target_size)));
	}

	return target_lod;
}

void Controller::garbageCollectPointsOfInterest()
{
	auto &points = m_points_of_interest;
	points.erase(std::remove_if(points.begin(), points.end(), [this](const PointOfInterest &point) {
		return m_tick_id - point.last_update_tick_id > MAX_POI_AGE;
	}), points.end());
}

Controller::ControlBlockPtr Controller::copyOnWrite(const ChunkControlBlock &cb, bool mark_seam_dirty)
{
	auto ptr = PoolAllocator::allocateControlBlock(ChunkControlBlock::CreationInfo {
		.predecessor = &cb,
		.reset_seam = mark_seam_dirty
	});

	if (mark_seam_dirty) {
		ptr->setSeamDirty(true);
	}

	m_new_cbs.emplace(ptr.get());
	return ptr;
}

Controller::ControlBlockPtr Controller::enqueueLoadingChunk(ChunkId id)
{
	auto chunk_ptr = PoolAllocator::allocateChunk(Chunk::CreationInfo {
		.id = id,
		.version = 0,
		.reuse_type = Chunk::ReuseType::Nothing,
		.reuse_chunk = nullptr
	});

	assert(!m_async_chunk_loads.contains(id));
	m_async_chunk_loads[id] = ThreadPool::globalVoxenPool().enqueueTask(ThreadPool::TaskType::Standard,
		[this, id]() {
			return m_loader.load(id);
		}
	);

	auto cb_ptr = PoolAllocator::allocateControlBlock(ChunkControlBlock::CreationInfo { .predecessor = nullptr });
	m_new_cbs.emplace(cb_ptr.get());

	cb_ptr->setState(ChunkControlBlock::State::Loading);
	cb_ptr->setChunk(std::move(chunk_ptr));
	return cb_ptr;
}

Controller::OuterUpdateResult Controller::updateChunk(const ChunkControlBlock &cb, ParentCommand parent_cmd)
{
	InnerUpdateResult self_update_result;
	switch (cb.state()) {
	case ChunkControlBlock::State::Loading:
		self_update_result = updateChunkLoading(cb, parent_cmd);
		break;
	case ChunkControlBlock::State::Standby:
		self_update_result = updateChunkStandby(cb, parent_cmd);
		break;
	case ChunkControlBlock::State::Active:
		self_update_result = updateChunkActive(cb, parent_cmd);
		break;
	default:
		assert(false);
	}

	ControlBlockPtr new_me;
	const ChunkControlBlock *me = &cb;

	if (self_update_result.first.has_value()) {
		// Copy-on-write if self-update requested so
		new_me = *std::move(self_update_result.first);
		me = new_me.get();

		if (!me) {
			// Chunk was unloaded, don't continue to its children
			return new_me;
		}
	}

	OuterUpdateResult children_update_results[8];
	bool child_updated = false;
	bool child_seam_dirty = false;
	for (int i = 0; i < 8; i++) {
		const ChunkControlBlock *child = me->child(i);
		if (child) {
			children_update_results[i] = updateChunk(*child, self_update_result.second);
			if (children_update_results[i].has_value()) {
				child_updated = true;
				if (*children_update_results[i]) {
					child_seam_dirty |= (*children_update_results[i])->isSeamDirty();
				}
			} else {
				child_seam_dirty |= child->isSeamDirty();
			}
		}
	}

	if (child_updated) {
		if (!new_me) {
			// Copy-on-write if any child requested so
			new_me = copyOnWrite(cb, child_seam_dirty);
		}

		for (int i = 0; i < 8; i++) {
			if (children_update_results[i].has_value()) {
				new_me->setChild(i, *std::move(children_update_results[i]));
			}
		}
	}

	if (child_seam_dirty && !me->isSeamDirty()) {
		// Some child has "seam dirty" flag, need to propagate it
		if (!new_me) {
			new_me = copyOnWrite(cb, true);
		} else {
			new_me->setSeamDirty(true);
		}
	}

	if (new_me) {
		return std::move(new_me);
	}

	return std::nullopt;
}

Controller::InnerUpdateResult Controller::updateChunkLoading(const ChunkControlBlock &cb, ParentCommand parent_cmd)
{
	assert(parent_cmd == ParentCommand::Nothing);
	(void) parent_cmd; // For builds with disabled asserts

	// TODO: this is just debug stub
	if (m_load_quota < MAX_DIRECT_OP_COUNT) {
		assert(cb.chunk());
		auto iter = m_async_chunk_loads.find(cb.chunk()->id());
		assert(iter != m_async_chunk_loads.end());
		assert(iter->second.valid());
		if (iter->second.wait_for(std::chrono::nanoseconds(0)) == std::future_status::ready) {
			auto ptr = copyOnWrite(cb);
			ptr->setState(ChunkControlBlock::State::Standby);
			ptr->setChunk(iter->second.get());
			m_async_chunk_loads.erase(iter);
			m_load_quota++;
			return { std::move(ptr), ParentCommand::Nothing };
		}
	}

	return { std::nullopt, ParentCommand::Nothing };
}

Controller::InnerUpdateResult Controller::updateChunkStandby(const ChunkControlBlock &cb, ParentCommand parent_cmd)
{
	assert(cb.chunk());

	if (cb.isOverActive()) {
		assert(parent_cmd == ParentCommand::Nothing);

		// We are over active, check if LOD deterioration is possible
		const ChunkId my_id = cb.chunk()->id();
		const uint32_t my_lod_dir = calcLodDirection(my_id);
		if (my_lod_dir < my_id.lod) {
			// Even if all children are willing to deteriorate their LOD,
			// this is pointless as we would then instantly improve it back
			return { std::nullopt, ParentCommand::Nothing };
		}

		for (unsigned i = 0; i < 8; i++) {
			const ChunkControlBlock *child_cb = cb.child(i);
			assert(child_cb);

			if (child_cb->state() != ChunkControlBlock::State::Active) {
				// At least one child is not active - can't deteriorate LOD
				return { std::nullopt, ParentCommand::Nothing };
			}

			const ChunkId child_id = my_id.toChild(i);
			const uint32_t child_lod_dir = calcLodDirection(child_id);
			if (child_lod_dir <= child_id.lod) {
				// This child does not want to deteriorate its LOD
				return { std::nullopt, ParentCommand::Nothing };
			}
		}

		// All checks are passed, we can safely deteriorate LOD
		auto ptr = copyOnWrite(cb, true);
		ptr->setState(ChunkControlBlock::State::Active);
		ptr->setOverActive(false);
		return { std::move(ptr), ParentCommand::BecomeStandby };
	}

	if (parent_cmd == ParentCommand::BecomeActive) {
		// Parent has improved LOD, this chunk becomes active
		auto ptr = copyOnWrite(cb, true);
		ptr->setState(ChunkControlBlock::State::Active);
		return { std::move(ptr), ParentCommand::Nothing };
	}

	assert(parent_cmd == ParentCommand::Nothing);

	// Try to unload this node if it's not over active, has no
	// children and is not expected to be used in LOD changing soon
	for (unsigned i = 0; i < 8; i++) {
		if (cb.child(i)) {
			return { std::nullopt, ParentCommand::Nothing };
		}
	}

	const ChunkId my_id = cb.chunk()->id();
	const uint32_t my_lod_dir = calcLodDirection(my_id);
	if (my_lod_dir > my_id.lod + 1) {
		// This chunk is not expected to be needed soon, can remove it
		m_loader.unload(cb.chunkPtr());
		return { ControlBlockPtr(), ParentCommand::Nothing };
	}

	return { std::nullopt, ParentCommand::Nothing };
}

Controller::InnerUpdateResult Controller::updateChunkActive(const ChunkControlBlock &cb, ParentCommand parent_cmd)
{
	assert(cb.chunk());

	if (parent_cmd == ParentCommand::BecomeStandby) {
		// Parent has decided to deteriorate LOD, this chunk becomes standby
		auto ptr = copyOnWrite(cb);
		ptr->setState(ChunkControlBlock::State::Standby);
		return { std::move(ptr), ParentCommand::Nothing };
	}

	assert(parent_cmd == ParentCommand::Nothing);

	const ChunkId my_id = cb.chunk()->id();
	const uint32_t my_lod_dir = calcLodDirection(my_id);
	if (my_lod_dir < my_id.lod) {
		// We need to improve LOD, check if we can do it
		ControlBlockPtr new_children[8];
		bool has_new_children = false;
		bool can_improve = true;

		for (unsigned i = 0; i < 8; i++) {
			const ChunkControlBlock *child_cb = cb.child(i);
			if (!child_cb) {
				new_children[i] = enqueueLoadingChunk(my_id.toChild(i));

				// Child is not present - request loading it immediately
				has_new_children = true;
				can_improve = false;
			} else if (child_cb->state() == ChunkControlBlock::State::Loading) {
				// Child is still loading
				can_improve = false;
			}
		}

		if (has_new_children) {
			// Can't improve LOD right now, wait for all children to become loaded
			auto ptr = copyOnWrite(cb);
			for (unsigned i = 0; i < 8; i++) {
				if (new_children[i]) {
					ptr->setChild(i, std::move(new_children[i]));
				}
			}
			return { std::move(ptr), ParentCommand::Nothing };
		}

		if (can_improve) {
			auto ptr = copyOnWrite(cb);
			ptr->setState(ChunkControlBlock::State::Standby);
			ptr->setOverActive(true);
			return { std::move(ptr), ParentCommand::BecomeActive };
		}
	}

	return { std::nullopt, ParentCommand::Nothing };
}

}
