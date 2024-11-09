#include <voxen/common/terrain/controller.hpp>

#include <voxen/common/terrain/allocator.hpp>
#include <voxen/common/terrain/chunk.hpp>
#include <voxen/common/terrain/coord.hpp>
#include <voxen/common/thread_pool.hpp>
#include <voxen/svc/service_locator.hpp>
#include <voxen/util/hash.hpp>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>

namespace voxen::terrain
{

Controller::Controller(svc::ServiceLocator &svc) : m_thread_pool(svc.requestService<ThreadPool>()) {}

Controller::~Controller()
{
	// Wait for pending async operations before destroying - they reference class members
	for (auto iter = m_async_chunk_loads.begin(); iter != m_async_chunk_loads.end(); /*nothing*/) {
		assert(iter->second.valid());
		iter->second.wait();
		iter = m_async_chunk_loads.erase(iter);
	}
}

std::vector<Controller::ChunkPtr> Controller::doTick()
{
	garbageCollectPointsOfInterest();
	engageSuperchunks();

	m_direct_op_quota = 0;

	// Walk over superchunks and update them
	for (auto iter = m_superchunks.begin(); iter != m_superchunks.end(); /* no change here */) {
		auto &info = iter->second;

		assert(info.ptr);
		ChunkControlBlock &cb = *info.ptr;

		info.idle_age++;
		if (info.idle_age > Config::SUPERCHUNK_MAX_AGE) {
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

		++iter;
	}

	std::vector<ChunkPtr> result;
	std::vector<const ChunkControlBlock *> stack;
	stack.reserve(8 * Config::CHUNK_MAX_LOD);

	for (auto &[_, info] : m_superchunks) {
		// Collect active list of this chunk.
		// TODO (Svenny): this can be optimized by keeping old list and "patching" it.
		stack.emplace_back(info.ptr.get());
		while (!stack.empty()) {
			const ChunkControlBlock *cb = stack.back();
			stack.pop_back();

			if (cb->state() == ChunkControlBlock::State::Active) {
				// Don't add surfaceless chunks to active list, greatly shortening it.
				// TODO (Svenny): this will have undesired consequences if some downstream
				// algorithm (ore scanning?) will require below-the-surface voxel data.
				if (cb->chunk()->hasSurface()) {
					result.emplace_back(cb->chunkPtr());
				}
				continue;
			}

			for (size_t i = 0; i < 8; i++) {
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
		.position = position,
	});
}

uint32_t Controller::calcLodDirection(land::ChunkKey id) const
{
	// This value multiplied by chunk side size gives an
	// average of inscribed and circumscribed spheres' radii
	constexpr double PSEUDORADIUS_MULT = (glm::root_two<double>() + 1.0) / 2.0;
	constexpr double OPTIMAL_PHI = glm::radians(Config::CHUNK_OPTIMAL_ANGULAR_SIZE_DEGREES);
	const double OPTIMAL_TAN_HALF_PHI = glm::tan(OPTIMAL_PHI * 0.5);

	// Radius of a sphere used to approximate angular diameter of a chunk
	const double delta = double(Config::CHUNK_SIZE << id.scale_log2) * PSEUDORADIUS_MULT;
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

	auto eraser = [](const PointOfInterest &point) { return point.age > Config::POINT_OF_INTEREST_MAX_AGE; };
	points.erase(std::remove_if(points.begin(), points.end(), eraser), points.end());
}

void Controller::engageSuperchunks()
{
	constexpr static double SUPERCHUNK_WORLD_SIZE = double(Config::CHUNK_SIZE << Config::CHUNK_MAX_LOD);

	auto &points = m_points_of_interest;

	for (auto &point : points) {
		// Subtracting 0.5 because integer coordinates are for superchunk
		// bases, but we want to check engagement against their centers
		const glm::dvec3 p = point.position / SUPERCHUNK_WORLD_SIZE;
		glm::ivec3 engage_min = glm::floor(p - (Config::SUPERCHUNK_ENGAGE_FACTOR + 0.5));
		glm::ivec3 engage_max = glm::ceil(p + (Config::SUPERCHUNK_ENGAGE_FACTOR - 0.5));

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
	base <<= Config::CHUNK_MAX_LOD;
	return enqueueLoadingChunk(land::ChunkKey(base, Config::CHUNK_MAX_LOD));
}

Controller::ControlBlockPtr Controller::enqueueLoadingChunk(land::ChunkKey id)
{
	// Chunk will be filled by `TerrainLoader`
	auto chunk_ptr = PoolAllocator::allocateChunk(Chunk::CreationInfo {
		.id = id,
		.version = 0,
	});

	assert(!m_async_chunk_loads.contains(id));
	m_async_chunk_loads[id] = m_thread_pool.enqueueTask(ThreadPool::TaskType::Standard,
		[this, chunk_ptr]() { m_loader.load(*chunk_ptr); });

	auto cb_ptr = std::make_unique<ChunkControlBlock>();
	cb_ptr->setState(ChunkControlBlock::State::Loading);
	cb_ptr->setChunk(std::move(chunk_ptr));
	return cb_ptr;
}

bool Controller::updateChunk(ChunkControlBlock &cb, ParentCommand parent_cmd)
{
	if (parent_cmd == ParentCommand::Unload) {
		m_loader.unload(cb.chunkPtr());
		for (size_t i = 0; i < 8; i++) {
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

	for (size_t i = 0; i < 8; i++) {
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
		}
	}

	return true;
}

Controller::InnerUpdateResult Controller::updateChunkLoading(ChunkControlBlock &cb, ParentCommand parent_cmd)
{
	assert(parent_cmd == ParentCommand::Nothing);
	(void) parent_cmd; // For builds with disabled asserts

	// TODO: this looks hacky
	assert(cb.chunk());
	auto iter = m_async_chunk_loads.find(cb.chunk()->id());
	assert(iter != m_async_chunk_loads.end());
	assert(iter->second.valid());

	if (iter->second.wait_for(std::chrono::nanoseconds(0)) == std::future_status::ready) {
		iter->second.get();
		cb.setState(ChunkControlBlock::State::Standby);
		m_async_chunk_loads.erase(iter);
		return { true, ParentCommand::Nothing };
	}

	return { true, ParentCommand::Nothing };
}

Controller::InnerUpdateResult Controller::updateChunkStandby(ChunkControlBlock &cb, ParentCommand parent_cmd)
{
	assert(cb.chunk());

	if (cb.isOverActive() && m_direct_op_quota < Config::TERRAIN_MAX_DIRECT_OP_COUNT) {
		assert(parent_cmd == ParentCommand::Nothing);

		// We are over active, check if LOD deterioration is possible
		const land::ChunkKey my_id = cb.chunk()->id();
		const uint32_t my_lod_dir = calcLodDirection(my_id);
		if (my_lod_dir < my_id.scale_log2) {
			// Even if all children are willing to deteriorate their LOD,
			// this is pointless as we would then instantly improve it back
			return { true, ParentCommand::Nothing };
		}

		for (size_t i = 0; i < 8; i++) {
			const ChunkControlBlock *child_cb = cb.child(i);
			assert(child_cb);

			if (child_cb->state() != ChunkControlBlock::State::Active) {
				// At least one child is not active - can't deteriorate LOD
				return { true, ParentCommand::Nothing };
			}

			const land::ChunkKey child_id = my_id.childLodKey(uint32_t(i));
			const uint32_t child_lod_dir = calcLodDirection(child_id);
			if (child_lod_dir <= child_id.scale_log2) {
				// This child does not want to deteriorate its LOD
				return { true, ParentCommand::Nothing };
			}
		}

		// All checks are passed, we can safely deteriorate LOD
		m_direct_op_quota++;
		cb.setState(ChunkControlBlock::State::Active);
		cb.setOverActive(false);
		cb.setChunkChanged(true);
		return { true, ParentCommand::BecomeStandby };
	}

	if (parent_cmd == ParentCommand::BecomeActive) {
		// Parent has improved LOD, this chunk becomes active
		cb.setState(ChunkControlBlock::State::Active);
		cb.setChunkChanged(true);
		return { true, ParentCommand::Nothing };
	}

	assert(parent_cmd == ParentCommand::Nothing);

	// Try to unload this node if it's not over active, has no
	// children and is not expected to be used in LOD changing soon
	for (size_t i = 0; i < 8; i++) {
		if (cb.child(i)) {
			return { true, ParentCommand::Nothing };
		}
	}

	const land::ChunkKey my_id = cb.chunk()->id();
	const uint32_t my_lod_dir = calcLodDirection(my_id);
	if (my_lod_dir > my_id.scale_log2 + 1) {
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

	const land::ChunkKey my_id = cb.chunk()->id();
	const uint32_t my_lod_dir = calcLodDirection(my_id);
	if (my_lod_dir < my_id.scale_log2 && m_direct_op_quota + 8 <= Config::TERRAIN_MAX_DIRECT_OP_COUNT) {
		// We need to improve LOD, check if we can do it
		ControlBlockPtr new_children[8];
		bool can_improve = true;

		for (size_t i = 0; i < 8; i++) {
			const ChunkControlBlock *child_cb = cb.child(i);
			if (!child_cb) {
				// Child is not present - request loading it immediately
				cb.setChild(i, enqueueLoadingChunk(my_id.childLodKey(uint32_t(i))));
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

		m_direct_op_quota += 8;
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

} // namespace voxen::terrain
