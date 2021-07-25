#include <voxen/common/terrain/controller.hpp>

#include <voxen/config.hpp>
#include <voxen/common/threadpool.hpp>
#include <voxen/common/terrain/allocator.hpp>
#include <voxen/common/terrain/chunk.hpp>
#include <voxen/common/terrain/coord.hpp>
#include <voxen/util/hash.hpp>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>

namespace voxen::terrain
{

constexpr static uint32_t SUPERCHUNK_MAX_LOD = 12;
// Maximal time, in ticks, after which non-updated point of interest will be discared
// TODO (Svenny): move to `terrain/config.hpp`
constexpr static uint64_t MAX_POI_AGE = 1000;
// Maximal number of direct (non-seam) chunk ops which can be queued during one tick
// TODO (Svenny): move to `terrain/config.hpp`
constexpr static uint32_t MAX_DIRECT_OP_COUNT = 32;
// World-space size of a superchunk
constexpr static double SUPERCHUNK_WORLD_SIZE = double(Config::CHUNK_SIZE << SUPERCHUNK_MAX_LOD);
// Distance (in world-space coordinates) from point of interest
// to superchunk center which will trigger loading the superchunk
constexpr static double SUPERCHUNK_ENGAGE_RADIUS = SUPERCHUNK_WORLD_SIZE * 0.75;
// Maximal time, in ticks, after which non-engaged superchunk will get unloaded
constexpr static uint32_t SUPERCHUNK_MAX_IDLE_AGE = 1000;

std::vector<Controller::ChunkPtr> Controller::doTick()
{
	garbageCollectPointsOfInterest();
	engageSuperchunks();

	m_load_quota = 0;

	// Walk over superchunks and update them
	for (auto iter = m_superchunks.begin(); iter != m_superchunks.end(); /* no change here */) {
		auto &info = iter->second;

		assert(info.ptr);
		ChunkControlBlock &cb = *info.ptr;

		info.idle_age++;
		if (info.idle_age > SUPERCHUNK_MAX_IDLE_AGE) {
			// Superchunk is out of interest and can be unloaded
			updateChunk(cb, ParentCommand::Unload);
			iter = m_superchunks.erase(iter);
			continue;
		}

		if (cb.isOverActive() || cb.state() == ChunkControlBlock::State::Active) {
			// When max-LOD chunk is loading, surface integrity ("active coverage")
			// is violated, so don't launch validation unless it's done
			cb.validateState();
		}

		ParentCommand cmd = ParentCommand::Nothing;
		if (cb.state() == ChunkControlBlock::State::Standby && !cb.isOverActive()) {
			// Root has loaded and now needs a "kick" to become active
			cmd = ParentCommand::BecomeActive;
		}

		bool update_result = updateChunk(cb, cmd);
		if (!update_result) {
			// Superchunk has unloaded itself
			iter = m_superchunks.erase(iter);
			continue;
		}

		// Phase 1 - induce "seam dirty" flags to seam-dependent chunks
		seamCellProcPhase1(&cb);

		++iter;
	}

	updateCrossSuperchunkSeams();

	std::vector<ChunkPtr> result;
	std::vector<const ChunkControlBlock *> stack;
	stack.reserve(8 * SUPERCHUNK_MAX_LOD);

	// Inner seams must be rebuilt strictly after cross-superchunk
	// ones because "seam dirty" flags are reset in Phase 2 pass
	for (auto &[_, info] : m_superchunks) {
		// Phase 2 - rebuild seams and clear temporary flags
		seamCellProcPhase2(info.ptr.get());

		// Collect active list of this chunk.
		// TODO (Svenny): this can be optimized by keeping old list and "patching" it.
		stack.emplace_back(info.ptr.get());
		while (!stack.empty()) {
			const ChunkControlBlock *cb = stack.back();
			stack.pop_back();

			if (cb->state() == ChunkControlBlock::State::Active) {
				result.emplace_back(cb->chunkPtr());
				continue;
			}

			for (int i = 0; i < 8; i++) {
				const ChunkControlBlock *child = cb->child(i);
				if (child) {
					stack.emplace_back(child);
				}
			}
		}
	}

	return result;
}

void Controller::setPointOfInterest(uint32_t id, const glm::dvec3 &position)
{
	for (auto &point : m_points_of_interest) {
		if (point.id == id) {
			point.age = 0;
			point.position = position;
			return;
		}
	}

	m_points_of_interest.emplace_back(PointOfInterest {
		.id = id,
		.age = 0,
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

	for (auto &point : points) {
		point.age++;
	}

	points.erase(std::remove_if(points.begin(), points.end(), [](const PointOfInterest &point) {
		return point.age > MAX_POI_AGE;
	}), points.end());
}

void Controller::engageSuperchunks()
{
	auto &points = m_points_of_interest;

	for (auto &point : points) {
		glm::ivec3 engage_min = glm::floor((point.position - SUPERCHUNK_ENGAGE_RADIUS) / SUPERCHUNK_WORLD_SIZE - 0.5);
		glm::ivec3 engage_max = glm::ceil((point.position + SUPERCHUNK_ENGAGE_RADIUS) / SUPERCHUNK_WORLD_SIZE - 0.5);

		for (int32_t y = engage_min.y; y <= engage_max.y; y++) {
			for (int32_t x = engage_min.x; x <= engage_max.x; x++) {
				for (int32_t z = engage_min.z; z <= engage_max.z; z++) {
					const glm::ivec3 base(x, y, z);

					auto &info = m_superchunks[base];
					info.idle_age = 0;

					if (!info.ptr) {
						info.ptr = loadSuperchunk(base);
					}
				}
			}
		}
	}
}

Controller::ControlBlockPtr Controller::loadSuperchunk(glm::ivec3 base)
{
	base <<= SUPERCHUNK_MAX_LOD;
	return enqueueLoadingChunk(ChunkId {
		.lod = SUPERCHUNK_MAX_LOD, .base_x = base.x, .base_y = base.y, .base_z = base.z
	});
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

	auto cb_ptr = std::make_unique<ChunkControlBlock>();
	cb_ptr->setState(ChunkControlBlock::State::Loading);
	cb_ptr->setChunk(std::move(chunk_ptr));
	return cb_ptr;
}

bool Controller::updateChunk(ChunkControlBlock &cb, ParentCommand parent_cmd)
{
	if (parent_cmd == ParentCommand::Unload) {
		m_loader.unload(cb.chunkPtr());
		for (int i = 0; i < 8; i++) {
			if (cb.child(i)) {
				updateChunk(*cb.child(i), parent_cmd);
			}
		}
		return false;
	}

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

	if (!self_update_result.first) {
		// Chunk was unloaded, don't proceed to it's children
		return false;
	}

	for (int i = 0; i < 8; i++) {
		ChunkControlBlock *child = cb.child(i);
		if (child) {
			bool res = updateChunk(*child, self_update_result.second);
			if (!res) {
				cb.setChild(i, nullptr);
				continue;
			}

			if (child->isChunkChanged()) {
				cb.setChunkChanged(true);
			}

			if (child->isInducedSeamDirty()) {
				cb.setInducedSeamDirty(true);
			}
		}
	}

	return true;
}

Controller::InnerUpdateResult Controller::updateChunkLoading(ChunkControlBlock &cb, ParentCommand parent_cmd)
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
			cb.setState(ChunkControlBlock::State::Standby);
			cb.setChunk(iter->second.get());
			m_async_chunk_loads.erase(iter);
			m_load_quota++;
			return { true, ParentCommand::Nothing };
		}
	}

	return { true, ParentCommand::Nothing };
}

Controller::InnerUpdateResult Controller::updateChunkStandby(ChunkControlBlock &cb, ParentCommand parent_cmd)
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
			return { true, ParentCommand::Nothing };
		}

		for (unsigned i = 0; i < 8; i++) {
			const ChunkControlBlock *child_cb = cb.child(i);
			assert(child_cb);

			if (child_cb->state() != ChunkControlBlock::State::Active) {
				// At least one child is not active - can't deteriorate LOD
				return { true, ParentCommand::Nothing };
			}

			const ChunkId child_id = my_id.toChild(i);
			const uint32_t child_lod_dir = calcLodDirection(child_id);
			if (child_lod_dir <= child_id.lod) {
				// This child does not want to deteriorate its LOD
				return { true, ParentCommand::Nothing };
			}
		}

		// All checks are passed, we can safely deteriorate LOD
		cb.setState(ChunkControlBlock::State::Active);
		cb.setOverActive(false);
		cb.setChunkChanged(true);
		cb.setInducedSeamDirty(true);
		return { true, ParentCommand::BecomeStandby };
	}

	if (parent_cmd == ParentCommand::BecomeActive) {
		// Parent has improved LOD, this chunk becomes active
		cb.setState(ChunkControlBlock::State::Active);
		cb.setChunkChanged(true);
		cb.setInducedSeamDirty(true);
		return { true, ParentCommand::Nothing };
	}

	assert(parent_cmd == ParentCommand::Nothing);

	// Try to unload this node if it's not over active, has no
	// children and is not expected to be used in LOD changing soon
	for (unsigned i = 0; i < 8; i++) {
		if (cb.child(i)) {
			return { true, ParentCommand::Nothing };
		}
	}

	const ChunkId my_id = cb.chunk()->id();
	const uint32_t my_lod_dir = calcLodDirection(my_id);
	if (my_lod_dir > my_id.lod + 1) {
		// This chunk is not expected to be needed soon, can remove it
		m_loader.unload(cb.chunkPtr());
		return { false, ParentCommand::Nothing };
	}

	return { true, ParentCommand::Nothing };
}

Controller::InnerUpdateResult Controller::updateChunkActive(ChunkControlBlock &cb, ParentCommand parent_cmd)
{
	assert(cb.chunk());

	if (parent_cmd == ParentCommand::BecomeStandby) {
		// Parent has decided to deteriorate LOD, this chunk becomes standby
		cb.setState(ChunkControlBlock::State::Standby);
		return { true, ParentCommand::Nothing };
	}

	assert(parent_cmd == ParentCommand::Nothing);

	const ChunkId my_id = cb.chunk()->id();
	const uint32_t my_lod_dir = calcLodDirection(my_id);
	if (my_lod_dir < my_id.lod) {
		// We need to improve LOD, check if we can do it
		ControlBlockPtr new_children[8];
		bool can_improve = true;

		for (int i = 0; i < 8; i++) {
			const ChunkControlBlock *child_cb = cb.child(i);
			if (!child_cb) {
				// Child is not present - request loading it immediately
				cb.setChild(i, enqueueLoadingChunk(my_id.toChild(i)));
				can_improve = false;
			} else if (child_cb->state() == ChunkControlBlock::State::Loading) {
				// Child is still loading
				can_improve = false;
			}
		}

		if (!can_improve) {
			// Can't improve LOD right now, wait for all children to become loaded
			return { true, ParentCommand::Nothing };
		}

		cb.setState(ChunkControlBlock::State::Standby);
		cb.setOverActive(true);
		return { true, ParentCommand::BecomeActive };
	}

	return { true, ParentCommand::Nothing };
}

size_t Controller::VecHasher::operator()(const glm::ivec3 &v) const noexcept
{
	return hashXorshift32(reinterpret_cast<const uint32_t *>(glm::value_ptr(v)), 3);
}

}