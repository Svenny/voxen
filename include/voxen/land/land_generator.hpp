#pragma once

#include <voxen/common/shared_object_pool.hpp>
#include <voxen/land/chunk_key.hpp>
#include <voxen/land/land_chunk.hpp>
#include <voxen/land/pseudo_chunk_data.hpp>
#include <voxen/svc/svc_fwd.hpp>
#include <voxen/util/lru_visit_ordering.hpp>
#include <voxen/world/world_tick_id.hpp>

namespace voxen::land
{

class GeneratorGlobalMap {
public:
	struct SampledPoint {
		float height;
		float temperature;
	};

	struct Point {
		int16_t height;
		int8_t temperature;
		uint8_t variance;
	};

	GeneratorGlobalMap() = default;
	GeneratorGlobalMap(GeneratorGlobalMap &&) = delete;
	GeneratorGlobalMap(const GeneratorGlobalMap &) = delete;
	GeneratorGlobalMap &operator=(GeneratorGlobalMap &&) = delete;
	GeneratorGlobalMap &operator=(const GeneratorGlobalMap &) = delete;
	~GeneratorGlobalMap() = default;

	uint64_t enqueueGenerate(uint64_t seed, svc::TaskBuilder &bld);

	SampledPoint sample(double x, double z) const noexcept;

private:
	int32_t m_width = 0;
	int32_t m_height = 0;
	std::unique_ptr<Point[]> m_points;

	void doGenerate(uint64_t seed);
};

class GeneratorRegionalMap {
public:
	struct Point {
		float height;
		float temperature;
		float variance;
	};

	GeneratorRegionalMap() = default;
	GeneratorRegionalMap(GeneratorRegionalMap &&) = delete;
	GeneratorRegionalMap(const GeneratorRegionalMap &) = delete;
	GeneratorRegionalMap &operator=(GeneratorRegionalMap &&) = delete;
	GeneratorRegionalMap &operator=(const GeneratorRegionalMap &) = delete;
	~GeneratorRegionalMap() = default;

	uint64_t enqueueGenerate(int32_t width, int32_t height, uint64_t seed, svc::TaskBuilder &bld);

private:
	int32_t m_width = 0;
	int32_t m_height = 0;
	std::unique_ptr<Point[]> m_points;
};

// This class has special multithreaded usage rules,
// see description of every function before using it.
class Generator {
public:
	Generator();
	Generator(Generator &&) = delete;
	Generator(const Generator &) = delete;
	Generator &operator=(Generator &&) = delete;
	Generator &operator=(const Generator &) = delete;
	~Generator();

	void onWorldTickBegin(world::TickId new_tick);
	void setSeed(uint64_t seed);
	// Uses `bld` to wait for tasks enqueued by this builder.
	// Call it before destroying to protect from use-after-free.
	void waitEnqueuedTasks(svc::TaskBuilder &bld);

	// Enqueues (potentially) an asynchronous task (using the provided task builder interface)
	// that will prepare resources needed to generate chunk indexed by `key`.
	// Returns waitable task counter of the launched task or zero. You should make
	// your generating task for this key wait on this counter before executing.
	uint64_t prepareKeyGeneration(ChunkKey key, svc::TaskBuilder &bld);

	// Generate a true (LOD0) chunk.
	// Should be called from asynchronous task - this takes a while.
	// Before launching that task, call `prepareKeyGeneration(key)`
	// and wait on the returned counter.
	//
	// `key` must have zero scale, be within world height limit and be wrapped
	// inside world non-repeating zone. Otherwise the behavior is undefined.
	void generateChunk(ChunkKey key, Chunk &output);

	// Generate a pseudo (LODn) chunk.
	// Should be called from asynchronous task - this takes a while.
	// Before launching that task, call `prepareKeyGeneration(key)`
	// and wait on the returned counter.
	//
	// `key` must have scale at least one (pseudo-chunks of LOD0 are not supported)
	// and at most `land::Consts::MAX_GENERATABLE_LOD`.
	void generatePseudoChunk(ChunkKey key, PseudoChunkData &output);

private:
	constexpr static uint32_t REGIONAL_MAP_POOL_HINT = 128;

	struct RegionalMapCacheEntry {
		SharedPoolPtr<GeneratorRegionalMap, REGIONAL_MAP_POOL_HINT> ptr;
		world::TickId last_referenced_tick = world::TickId::INVALID;
		uint64_t gen_task_counter = 0;
	};

	uint64_t m_initial_seed = 0;

	uint64_t m_global_map_sub_seed = 0;
	uint64_t m_regional_map_sub_seed = 0;
	uint64_t m_local_noise_sub_seed = 0;

	GeneratorGlobalMap m_global_map;

	world::TickId m_current_world_tick = world::TickId::INVALID;
	uint64_t m_global_map_gen_task_counter = 0;

	SharedObjectPool<GeneratorRegionalMap, REGIONAL_MAP_POOL_HINT> m_regional_map_pool;

	uint64_t ensureGlobalMap(svc::TaskBuilder &bld);
};

} // namespace voxen::land
