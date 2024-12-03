#include <voxen/land/land_service.hpp>

#include <voxen/common/shared_object_pool.hpp>
#include <voxen/debug/uid_registry.hpp>
#include <voxen/land/land_messages.hpp>
#include <voxen/land/land_utils.hpp>
#include <voxen/svc/messaging_service.hpp>
#include <voxen/svc/service_locator.hpp>
#include <voxen/svc/task_builder.hpp>
#include <voxen/svc/task_service.hpp>
#include <voxen/util/concentric_octahedra_walker.hpp>
#include <voxen/util/log.hpp>
#include <voxen/util/lru_visit_ordering.hpp>

#include "land_private_consts.hpp"
#include "land_private_messages.hpp"

#include <array>
#include <unordered_map>
#include <vector>

namespace voxen::land
{

static_assert(Consts::NUM_LOD_SCALES <= 1u << Consts::CHUNK_KEY_SCALE_BITS, "LOD scales don't fit in ChunkKey bits");

using ChunkPtr = LandState::ChunkTable::ValuePtr;
using PseudoDataPtr = SharedPoolPtr<PseudoChunkData>;
using PseudoSurfacePtr = LandState::PseudoChunkSurfaceTable::ValuePtr;

namespace
{

constexpr double Y_BAND_LIMIT = 90.0;

double surfaceFn(glm::dvec3 point)
{
	constexpr std::tuple<double, double, double, double> octaves[] = {
		{ 4.0, 0.5, 0.03, 0.09 },
		{ 8.0, 3.5, -0.013, 0.048 },
		{ 16.0, 14.1, 0.0095, -0.0205 },
		{ 12.0, -7.5, -0.08, 0.0333 },
		{ 64.0, 7.65, 0.007, 0.0032 },
	};

	double fn = 0.0;
	for (const auto &[amp, phi, fx, fz] : octaves) {
		fn += amp * sin(phi + point.x * fx + point.z * fz);
	}

	//fn *= 0.000001;

	//fn += 40.0;
	//fn += 12.0;

	return fn;
}

void generateChunk(ChunkKey key, ChunkPtr &out_ptr)
{
	constexpr int32_t BLOCKS = Consts::CHUNK_SIZE_BLOCKS;

	const glm::ivec3 coords = key.base();
	const glm::ivec3 first_block_coords = coords * BLOCKS;

	// Quickly check if the chunk is fully above/below the surface
	{
		const double ymin = double(first_block_coords.y) * Consts::BLOCK_SIZE_METRES;
		if (ymin > Y_BAND_LIMIT) {
			// Fully above, all zeros
			out_ptr->setAllBlocksUniform(0);
			return;
		}

		const double ymax = double(first_block_coords.y + (BLOCKS << key.scale_log2)) * Consts::BLOCK_SIZE_METRES;
		if (ymax < -Y_BAND_LIMIT) {
			// Fully below, all ones
			out_ptr->setAllBlocksUniform(1);
			return;
		}
	}

	// Allocate on heap, expanded array is pretty large
	auto ids = std::make_unique<Chunk::BlockIdArray>();
	bool empty = true;

	const int32_t step = key.scaleMultiplier();
	const double halfstep = double(step) * 0.5;

	Utils::forYXZ<BLOCKS>([&](uint32_t x, uint32_t y, uint32_t z) {
		const glm::ivec3 pos = glm::ivec3(x, y, z);
		const glm::ivec3 block = first_block_coords + pos * step;
		const glm::dvec3 block_world = (glm::dvec3(block) + halfstep) * Consts::BLOCK_SIZE_METRES;

		double fn = surfaceFn(block_world);

		if (fn > block_world.y) {
			empty = false;
			(*ids)[pos] = 1;
		} else {
			(*ids)[pos] = 0;
		}
	});

	if (empty) {
		out_ptr->setAllBlocksUniform(0);
		return;
	}

	out_ptr->setAllBlocks(ids->cview());
}

void loadChunk(ChunkKey key, svc::MessageSender *sender, ChunkPtr out_ptr)
{
	assert(key.scale_log2 == 0);
	generateChunk(key, out_ptr);
	sender->send<detail::ChunkLoadCompletionMessage>(LandService::SERVICE_UID, key, std::move(out_ptr));
}

void generateImpostor(ChunkKey key, std::array<ChunkPtr, 7> ref, svc::MessageSender *sender, PseudoDataPtr out_ptr)
{
	// Must not be called with empty main chunk
	assert(ref[0]);

	ChunkAdjacencyRef adj(*ref[0]);
	for (size_t i = 0; i < 6; i++) {
		adj.adjacent[i] = ref[i + 1].get();
	}

	*out_ptr = PseudoChunkData(adj);
	sender->send<detail::PseudoChunkDataGenCompletionMessage>(LandService::SERVICE_UID, key);
}

void generatePseudoSurface(ChunkKey key, std::array<PseudoDataPtr, 7> ref, svc::MessageSender *sender)
{
	// Must not be called with empty main chunk
	assert(ref[0]);

#if 0
	std::array<const PseudoChunkData *, 6> adjacent;
	for (size_t i = 0; i < 6; i++) {
		adjacent[i] = ref[i + 1].get();
	}

	auto out_ptr = LandState::PseudoChunkSurfaceTable::makeValuePtr(PseudoChunkSurface(*ref[0]));

	if (!out_ptr->empty()) {
		// At least one face exists, then non-empty impostor too
		sender->send<detail::PseudoChunkSurfaceGenCompletionMessage>(LandService::SERVICE_UID, key, std::move(out_ptr));
	} else {
		// Empty impostor, no need to add it to the tree
		sender->send<detail::PseudoChunkSurfaceGenCompletionMessage>(LandService::SERVICE_UID, key);
	}
#else
	// TODO: temporary debug stuff

	if (ref[0]->empty()) {
		sender->send<detail::PseudoChunkSurfaceGenCompletionMessage>(LandService::SERVICE_UID, key);
	} else {
		auto out_ptr = LandState::PseudoChunkSurfaceTable::makeValuePtr(PseudoChunkSurface(*ref[0]));
		sender->send<detail::PseudoChunkSurfaceGenCompletionMessage>(LandService::SERVICE_UID, key, std::move(out_ptr));
	}
#endif
}

void generatePseudoChunk(ChunkKey key, svc::MessageSender *sender, PseudoDataPtr out_ptr)
{
	std::array<ChunkPtr, 7> adj;
	for (auto &ptr : adj) {
		ptr = LandState::ChunkTable::makeValuePtr();
	}

	generateChunk(key, adj[0]);

	const int32_t step = key.scaleMultiplier();
	generateChunk(ChunkKey(key.x + step, key.y, key.z, key.scale_log2), adj[1]);
	generateChunk(ChunkKey(key.x - step, key.y, key.z, key.scale_log2), adj[2]);
	generateChunk(ChunkKey(key.x, key.y + step, key.z, key.scale_log2), adj[3]);
	generateChunk(ChunkKey(key.x, key.y - step, key.z, key.scale_log2), adj[4]);
	generateChunk(ChunkKey(key.x, key.y, key.z + step, key.scale_log2), adj[5]);
	generateChunk(ChunkKey(key.x, key.y, key.z - step, key.scale_log2), adj[6]);

	generateImpostor(key, std::move(adj), sender, std::move(out_ptr));
}

void generateImpostor8(ChunkKey key, std::array<PseudoDataPtr, 8> lower, svc::MessageSender *sender,
	PseudoDataPtr out_ptr)
{
	const PseudoChunkData *ptrs[8];
	for (size_t i = 0; i < 8; i++) {
		ptrs[i] = lower[i].get();
	}

	*out_ptr = PseudoChunkData(ptrs);
	sender->send<detail::PseudoChunkDataGenCompletionMessage>(LandService::SERVICE_UID, key);
}

constexpr int64_t STALE_CHUNK_AGE_THRESHOLD = 750;

struct ChunkMetastate {
	WorldTickId last_referenced_tick = WorldTickId::INVALID;

	uint32_t pending_task_count : 8 = 0;
	uint32_t chunk_data_invalidated : 1 = 0;
	uint32_t pseudo_data_invalidated : 1 = 0;
	uint32_t pseudo_surface_invalidated : 1 = 0;
	uint32_t is_virgin : 1 = 1;

	uint64_t chunk_gen_task_counter = 0;
	uint64_t pseudo_data_gen_task_counter = 0;
	uint64_t pseudo_surface_gen_task_counter = 0;

	ChunkPtr latest_chunk_ptr;
	PseudoDataPtr latest_pseudo_data_ptr;
};

struct TicketState {
	ChunkTicketArea area;
	bool valid;
};

} // namespace

class detail::LandServiceImpl {
public:
	LandServiceImpl(svc::ServiceLocator &svc) : m_task_service(svc.requestService<svc::TaskService>())
	{
		// Public messages
		debug::UidRegistry::registerLiteral(ChunkTicketRequestMessage::MESSAGE_UID,
			"voxen::land::ChunkTicketRequestMessage");

		// Private messages
		debug::UidRegistry::registerLiteral(ChunkTicketAdjustMessage::MESSAGE_UID,
			"voxen::land::detail::ChunkTicketAdjustMessage");
		debug::UidRegistry::registerLiteral(ChunkTicketRemoveMessage::MESSAGE_UID,
			"voxen::land::detail::ChunkTicketRemoveMessage");
		debug::UidRegistry::registerLiteral(ChunkLoadCompletionMessage::MESSAGE_UID,
			"voxen::land::detail::ChunkLoadCompletionMessage");
		debug::UidRegistry::registerLiteral(PseudoChunkDataGenCompletionMessage::MESSAGE_UID,
			"voxen::land::detail::PseudoChunkDataGenCompletionMessage");
		debug::UidRegistry::registerLiteral(PseudoChunkSurfaceGenCompletionMessage::MESSAGE_UID,
			"voxen::land::detail::PseudoChunkSurfaceGenCompletionMessage");

		// Special UIDs
		debug::UidRegistry::registerLiteral(Consts::LAND_SERVICE_SENDER_UID,
			"voxen::land::Consts::LAND_SERVICE_SENDER_UID");

		auto &msgs = svc.requestService<svc::MessagingService>();
		m_queue = msgs.registerAgent(LandService::SERVICE_UID);
		m_sender = msgs.createSender(Consts::LAND_SERVICE_SENDER_UID);

		m_queue.registerHandler<ChunkTicketRequestMessage>(
			[this](ChunkTicketRequestMessage &msg, svc::MessageInfo &info) { handleChunkTicketRequest(msg, info); });
		m_queue.registerHandler<ChunkTicketAdjustMessage>(
			[this](ChunkTicketAdjustMessage &msg, svc::MessageInfo &) { handleChunkTicketAdjust(msg); });
		m_queue.registerHandler<ChunkTicketRemoveMessage>(
			[this](ChunkTicketRemoveMessage &msg, svc::MessageInfo &) { handleChunkTicketRemove(msg); });
		m_queue.registerHandler<ChunkLoadCompletionMessage>(
			[this](ChunkLoadCompletionMessage &msg, svc::MessageInfo &) { handleChunkLoadCompletion(msg); });
		m_queue.registerHandler<PseudoChunkDataGenCompletionMessage>(
			[this](PseudoChunkDataGenCompletionMessage &msg, svc::MessageInfo &) { handlePseudoDataGenCompletion(msg); });
		m_queue.registerHandler<PseudoChunkSurfaceGenCompletionMessage>(
			[this](PseudoChunkSurfaceGenCompletionMessage &msg, svc::MessageInfo &) {
				handlePseudoSurfaceGenCompletion(msg);
			});
	}

	~LandServiceImpl()
	{
		std::vector<uint64_t> wait_counters;
		wait_counters.reserve(m_metastate.size() * 3);

		// Jobs can reference this object, wait for completion before destroying.
		for (auto &item : m_metastate) {
			wait_counters.emplace_back(item.second.chunk_gen_task_counter);
			wait_counters.emplace_back(item.second.pseudo_data_gen_task_counter);
			wait_counters.emplace_back(item.second.pseudo_surface_gen_task_counter);
		}

		// We inserted A LOT of counters, let's quickly trim the set
		size_t remaining = m_task_service.eliminateCompletedWaitCounters(wait_counters);

		if (remaining > 0) {
			Log::debug("Waiting for pending Land jobs...");

			svc::TaskBuilder bld(m_task_service);
			bld.addWait(std::span(wait_counters.data(), remaining));
			bld.enqueueSyncPoint().wait();
		}

		m_queue.pollMessages();
	}

	void doTick(WorldTickId tick_id)
	{
		m_tick_id = tick_id;

		// Process chunk ticket change requests, now we have a fresh list of tickets.
		// Job completions and invalidation enqueues will be processed here too.
		m_queue.pollMessages();

		// Process data invalidations
		for (ChunkKey key : m_this_tick_pseudo_data_invalidations) {
			auto iter = m_metastate.find(key);
			if (iter == m_metastate.end()) {
				continue;
			}

			iter->second.pseudo_data_invalidated = 1;
			iter->second.is_virgin = 0;
		}

		for (ChunkKey key : m_this_tick_pseudo_surface_invalidations) {
			auto iter = m_metastate.find(key);
			if (iter == m_metastate.end()) {
				continue;
			}

			iter->second.pseudo_surface_invalidated = 1;
		}

		m_this_tick_pseudo_data_invalidations.clear();
		m_this_tick_pseudo_surface_invalidations.clear();

		// No keys left to update for this tick, collect a new list.
		// It might be very big if there are many tickets but we will consume
		// it in batches over the following ticks.
		// XXX: still not very got, can hitch on high workloads (too many players/chunkloading entities)
		if (m_keys_to_update.empty()) {
			for (const TicketState &state : m_chunk_tickets) {
				if (!state.valid) {
					continue;
				}

				if (const auto *box_area = std::get_if<ChunkTicketBoxArea>(&state.area); box_area != nullptr) {
					const ChunkKey lo = box_area->begin;
					const ChunkKey hi = box_area->end;
					const int32_t step = lo.scaleMultiplier();

					// Limit to vertical world bounds
					const int64_t lo_y = std::max<int64_t>(lo.y, Consts::MIN_WORLD_Y_CHUNK);
					const int64_t hi_y = std::min<int64_t>(hi.y, Consts::MAX_WORLD_Y_CHUNK);

					for (int64_t y = lo_y; y < hi_y; y += step) {
						for (int64_t x = lo.x; x < hi.x; x += step) {
							for (int64_t z = lo.z; z < hi.z; z += step) {
								ChunkKey ck(x, y, z, lo.scale_log2);
								m_keys_to_update.emplace_back(ck);
							}
						}
					}
				} else {
					const auto *octa_area = std::get_if<ChunkTicketOctahedronArea>(&state.area);
					// It can be either box or octahedron
					assert(octa_area != nullptr);

					const glm::ivec3 pivot = octa_area->pivot.base();
					const int32_t scale = octa_area->pivot.scaleMultiplier();

					ConcentricOctahedraWalker cwk(octa_area->scaled_radius);
					while (!cwk.wrappedAround()) {
						ChunkKey ck(pivot + scale * cwk.step(), octa_area->pivot.scale_log2);

						if (ck.y >= Consts::MIN_WORLD_Y_CHUNK && ck.y <= Consts::MAX_WORLD_Y_CHUNK) {
							m_keys_to_update.emplace_back(ck);
						}
					}
				}
			}

			// Eliminate duplicate keys from overlapping tickets
			std::sort(m_keys_to_update.begin(), m_keys_to_update.end());
			auto last = std::unique(m_keys_to_update.begin(), m_keys_to_update.end());
			m_keys_to_update.erase(last, m_keys_to_update.end());
		}

		// Limit the number of keys visited per tick.
		// TODO: move to constants/options/auto-adjust?
		constexpr size_t KEYS_PER_TICK = 500;
		const size_t num_visited = std::min(KEYS_PER_TICK, m_keys_to_update.size());

		for (size_t i = 0; i < num_visited; i++) {
			tickChunkKey(m_keys_to_update.back(), tick_id);
			m_keys_to_update.pop_back();
		}

		// Try cleaning up some unused chunks
		m_keys_lru_check_order.visitOldest(
			[&](ChunkKey key) -> WorldTickId {
				auto iter = m_metastate.find(key);
				if (iter == m_metastate.end()) {
					// Wut, key gone without our action?
					return WorldTickId::INVALID;
				}

				if (iter->second.last_referenced_tick + STALE_CHUNK_AGE_THRESHOLD > tick_id) {
					// Not yet stale, reschedule the visit
					return iter->second.last_referenced_tick + STALE_CHUNK_AGE_THRESHOLD;
				}

				if (iter->second.pending_task_count > 0) {
					// Has some pending work, unsafe to remove.
					// This will leave it pretty much at the same place - the chunk
					// itself is stale, we just need to wait for jobs completion.
					return tick_id + 1;
				}

				uint64_t version = static_cast<uint64_t>(tick_id.value);
				m_land_state.chunk_table.erase(version, iter->first);
				m_land_state.pseudo_chunk_surface_table.erase(version, iter->first);
				m_metastate.erase(iter);

				return WorldTickId::INVALID;
			},
			// TODO: move to constants/options
			1000, tick_id);
	}

	const LandState &landState() const noexcept { return m_land_state; }

private:
	svc::TaskService &m_task_service;

	svc::MessageQueue m_queue;
	svc::MessageSender m_sender;

	std::vector<TicketState> m_chunk_tickets;
	// Must be placed before any object that can store pool pointers
	// to destroy earlier. In our case this is only `m_metastate`.
	SharedObjectPool<PseudoChunkData> m_pseudo_chunk_data_pool;

	std::unordered_map<ChunkKey, ChunkMetastate> m_metastate;
	std::vector<ChunkKey> m_this_tick_pseudo_data_invalidations;
	std::vector<ChunkKey> m_this_tick_pseudo_surface_invalidations;

	LruVisitOrdering<ChunkKey, WorldTickTag> m_keys_lru_check_order;
	std::vector<ChunkKey> m_keys_to_update;

	WorldTickId m_tick_id;
	LandState m_land_state;

	ChunkMetastate &getMetastate(ChunkKey key)
	{
		auto [iter, inserted] = m_metastate.try_emplace(key);
		if (inserted) {
			// Register this key in cleanup visit ordering
			m_keys_lru_check_order.addKey(key, m_tick_id + STALE_CHUNK_AGE_THRESHOLD);
		}

		ChunkMetastate &m = iter->second;
		m.last_referenced_tick = m_tick_id;

		return m;
	}

	void tickChunkKey(ChunkKey ck, WorldTickId tick_id)
	{
		auto [iter, inserted] = m_metastate.try_emplace(ck);
		if (inserted) {
			// Register this key in cleanup visit ordering
			m_keys_lru_check_order.addKey(ck, tick_id + STALE_CHUNK_AGE_THRESHOLD);
		}

		ChunkMetastate &m = iter->second;
		m.last_referenced_tick = tick_id;

		if (ck.scale_log2 == 0) {
			enqueueChunkDataGen(ck, m);
		}

		enqueuePseudoSurfaceGen(ck, m);
	}

	void enqueuePseudoSurfaceGen(ChunkKey ck, ChunkMetastate &m)
	{
		if (!m.pseudo_surface_invalidated && m.pseudo_surface_gen_task_counter > 0) {
			// Surface is already generated and was not invalidated ever since
			return;
		}

		// Collect pseudo data from this and 6 adjacent chunks
		// TODO: do actually collect adjacent chunks
		// TODO: do insert wait on those chunks' gen tasks counters
		enqueuePseudoDataGen(ck, m);

		std::array<PseudoDataPtr, 7> dependencies;
		dependencies[0] = m.latest_pseudo_data_ptr;

		m.pending_task_count++;
		m.pseudo_surface_invalidated = 0;

		svc::TaskBuilder bld(m_task_service);
		// TODO: wait on all adjacent chunks datas too (dependencies)
		bld.addWait(m.pseudo_data_gen_task_counter);
		// This will ensure successive pseudo surface gen tasks complete in order
		bld.addWait(m.pseudo_surface_gen_task_counter);
		bld.enqueueTask([ck, deps = std::move(dependencies), snd = &m_sender](svc::TaskContext &) {
			generatePseudoSurface(ck, std::move(deps), snd);
		});
		m.chunk_gen_task_counter = bld.getLastTaskCounter();
	}

	void enqueuePseudoDataGen(ChunkKey ck, ChunkMetastate &m)
	{
		if (!m.pseudo_data_invalidated && m.pseudo_data_gen_task_counter > 0) {
			// Pseudo data is already generated and was not invalidated ever since
			return;
		}

		m.pending_task_count++;
		m.pseudo_data_invalidated = 0;
		m.latest_pseudo_data_ptr = m_pseudo_chunk_data_pool.allocate();

		svc::TaskBuilder bld(m_task_service);
		// This will ensure successive pseudo data gen tasks complete in order
		bld.addWait(m.pseudo_data_gen_task_counter);

		if (ck.scale_log2 == 0) {
			// LOD0 - collect chunk data from this and 6 adjacent chunks
			// TODO: optimize for case when all chunks are known to be
			// empty and will not produce any pseudo data. To know that
			// in advance there must be no pending gen tasks on them.
			std::array<ChunkPtr, 7> dependencies;
			uint64_t wait_counters[7] = {};

			enqueueChunkDataGen(ck, m);
			dependencies[0] = m.latest_chunk_ptr;
			wait_counters[0] = m.chunk_gen_task_counter;

			auto collect_dependency = [&](ChunkKey dk, size_t index) {
				if (dk.y < Consts::MIN_WORLD_Y_CHUNK || dk.y > Consts::MAX_WORLD_Y_CHUNK) {
					// Out of world height bounds, place null pointer meaning empty chunk (no data)
					// TODO: should place fake all-zeros chunk above the limit, all-blocks below the limit.
					return;
				}

				ChunkMetastate &mm = getMetastate(dk);
				enqueueChunkDataGen(dk, mm);
				dependencies[index] = mm.latest_chunk_ptr;
				wait_counters[index] = mm.chunk_gen_task_counter;
			};

			const glm::ivec3 B = ck.base();

			collect_dependency(ChunkKey(B + glm::ivec3(1, 0, 0)), 1);
			collect_dependency(ChunkKey(B - glm::ivec3(1, 0, 0)), 2);
			collect_dependency(ChunkKey(B + glm::ivec3(0, 1, 0)), 3);
			collect_dependency(ChunkKey(B - glm::ivec3(0, 1, 0)), 4);
			collect_dependency(ChunkKey(B + glm::ivec3(0, 0, 1)), 5);
			collect_dependency(ChunkKey(B - glm::ivec3(0, 0, 1)), 6);

			bld.addWait(wait_counters);
			bld.enqueueTask(
				[ck, deps = std::move(dependencies), snd = &m_sender, ptr = m.latest_pseudo_data_ptr](svc::TaskContext &) {
					generateImpostor(ck, std::move(deps), snd, std::move(ptr));
				});
		} else if (ck.scale_log2 <= Consts::MAX_DIRECT_GENERATE_LOD && m.is_virgin) {
			// Direct gen of "virgin" chunk - enqueue an independent task
			bld.enqueueTask([ck, snd = &m_sender, ptr = m.latest_pseudo_data_ptr](svc::TaskContext &) {
				generatePseudoChunk(ck, snd, std::move(ptr));
			});
		} else {
			// Aggregation gen - collect chunk data from 8 "children" chunks
			// TODO: optimize for case when all pseudochunks are known to be
			// empty and aggregation will not produce any data. To know that
			// in advance there must be no pending gen tasks on them.
			std::array<PseudoDataPtr, 8> dependencies;
			uint64_t wait_counters[8] = {};

			auto collect_dependency = [&](ChunkKey dk, size_t index) {
				if (dk.y < Consts::MIN_WORLD_Y_CHUNK || dk.y > Consts::MAX_WORLD_Y_CHUNK) {
					// Out of world height bounds, place null pointer meaning empty chunk (no pseudo data)
					return;
				}

				ChunkMetastate &mm = getMetastate(dk);
				enqueuePseudoDataGen(dk, mm);
				dependencies[index] = mm.latest_pseudo_data_ptr;
				wait_counters[index] = mm.pseudo_data_gen_task_counter;
			};

			const glm::ivec3 B = ck.base();
			const uint32_t S = ck.scale_log2 - 1u;
			const int32_t K = ck.scaleMultiplier() / 2;

			collect_dependency(ChunkKey(B + glm::ivec3(0, 0, K), S), 0);
			collect_dependency(ChunkKey(B + glm::ivec3(K, 0, K), S), 1);
			collect_dependency(ChunkKey(B + glm::ivec3(0, K, K), S), 2);
			collect_dependency(ChunkKey(B + glm::ivec3(K, K, K), S), 3);
			collect_dependency(ChunkKey(B + glm::ivec3(0, 0, 0), S), 4);
			collect_dependency(ChunkKey(B + glm::ivec3(K, 0, 0), S), 5);
			collect_dependency(ChunkKey(B + glm::ivec3(0, K, 0), S), 6);
			collect_dependency(ChunkKey(B + glm::ivec3(K, K, 0), S), 7);

			bld.addWait(wait_counters);
			bld.enqueueTask(
				[ck, deps = std::move(dependencies), snd = &m_sender, ptr = m.latest_pseudo_data_ptr](svc::TaskContext &) {
					generateImpostor8(ck, std::move(deps), snd, std::move(ptr));
				});
		}

		m.pseudo_data_gen_task_counter = bld.getLastTaskCounter();
	}

	void enqueueChunkDataGen(ChunkKey ck, ChunkMetastate &m)
	{
		assert(ck.scale_log2 == 0);

		if (!m.chunk_data_invalidated && m.chunk_gen_task_counter > 0) {
			// Chunk data is already generated and was not invalidated ever since
			return;
		}

		m.pending_task_count++;
		m.chunk_data_invalidated = 0;
		m.latest_chunk_ptr = LandState::ChunkTable::makeValuePtr();

		svc::TaskBuilder bld(m_task_service);
		// This will ensure successive chunk gen tasks complete in order
		bld.addWait(m.chunk_gen_task_counter);
		bld.enqueueTask(
			[ck, snd = &m_sender, ptr = m.latest_chunk_ptr](svc::TaskContext &) { loadChunk(ck, snd, std::move(ptr)); });
		m.chunk_gen_task_counter = bld.getLastTaskCounter();
	}

	void handleChunkTicketRequest(ChunkTicketRequestMessage &msg, svc::MessageInfo &info)
	{
		if (!validateChunkTicketArea(msg.area)) {
			Log::warn("Bad chunk ticket request came from {}; returning null handle",
				debug::UidRegistry::lookup(info.senderUid()));
			msg.ticket = {};
			return;
		}

		for (uint64_t ticket_id = 0; ticket_id < m_chunk_tickets.size(); ticket_id++) {
			if (!m_chunk_tickets[ticket_id].valid) {
				m_chunk_tickets[ticket_id].area = msg.area;
				m_chunk_tickets[ticket_id].valid = true;
				msg.ticket = ChunkTicket(ticket_id, &m_sender);
				return;
			}
		}

		m_chunk_tickets.emplace_back(TicketState { .area = msg.area, .valid = true });
		msg.ticket = ChunkTicket(m_chunk_tickets.size() - 1, &m_sender);
	}

	void handleChunkTicketAdjust(const ChunkTicketAdjustMessage &msg)
	{
		if (!validateChunkTicketArea(msg.new_area)) {
			// Sender is unknown - this message comes from our special ticket sender
			Log::warn("Bad chunk ticket adjust request for ticket ID {}; ticket not changed", msg.ticket_id);
			return;
		}

		assert(msg.ticket_id < m_chunk_tickets.size());
		m_chunk_tickets[msg.ticket_id].area = msg.new_area;
	}

	void handleChunkTicketRemove(const ChunkTicketRemoveMessage &msg)
	{
		assert(msg.ticket_id < m_chunk_tickets.size());
		m_chunk_tickets[msg.ticket_id].valid = false;
	}

	void handleChunkLoadCompletion(ChunkLoadCompletionMessage &msg)
	{
		ChunkMetastate &m = m_metastate[msg.key];
		m.pending_task_count--;
		m_land_state.chunk_table.insert(static_cast<uint64_t>(m_tick_id.value), msg.key, std::move(msg.value_ptr));

		// TODO: for chunk modifications (not full data gen) invalidate not every
		// adjacent pseudochunk data, but only those "touched" by this chunk.
		glm::ivec3 base = msg.key.base();

		m_this_tick_pseudo_data_invalidations.emplace_back(msg.key);
		m_this_tick_pseudo_data_invalidations.emplace_back(ChunkKey(base + glm::ivec3(1, 0, 0)));
		m_this_tick_pseudo_data_invalidations.emplace_back(ChunkKey(base - glm::ivec3(1, 0, 0)));
		m_this_tick_pseudo_data_invalidations.emplace_back(ChunkKey(base + glm::ivec3(0, 1, 0)));
		m_this_tick_pseudo_data_invalidations.emplace_back(ChunkKey(base - glm::ivec3(0, 1, 0)));
		m_this_tick_pseudo_data_invalidations.emplace_back(ChunkKey(base + glm::ivec3(0, 0, 1)));
		m_this_tick_pseudo_data_invalidations.emplace_back(ChunkKey(base - glm::ivec3(0, 0, 1)));
	}

	void handlePseudoDataGenCompletion(PseudoChunkDataGenCompletionMessage &msg)
	{
		ChunkMetastate &m = m_metastate[msg.key];
		m.pending_task_count--;

		// Invalidate pseudo surface of this chunk and pseudo data of its parent (reaggregation)
		m_this_tick_pseudo_surface_invalidations.emplace_back(msg.key);
		m_this_tick_pseudo_data_invalidations.emplace_back(msg.key.parentLodKey());
	}

	void handlePseudoSurfaceGenCompletion(PseudoChunkSurfaceGenCompletionMessage &msg)
	{
		ChunkMetastate &m = m_metastate[msg.key];
		m.pending_task_count--;

		m_land_state.pseudo_chunk_surface_table.insert(static_cast<uint64_t>(m_tick_id.value), msg.key,
			std::move(msg.value_ptr));
	}

	// Check that area requested for a chunk ticket is not empty and is within world bounds
	static bool validateChunkTicketArea(const ChunkTicketArea &area) noexcept
	{
		if (const auto *box_area = std::get_if<ChunkTicketBoxArea>(&area); box_area != nullptr) {
			ChunkKey lo = box_area->begin;
			ChunkKey hi = box_area->end;
			assert(lo.valid());
			assert(hi.valid());

			if (lo.scale_log2 >= Consts::NUM_LOD_SCALES) {
				Log::warn("Bad chunk ticket request: LOD {} outside of acceptable [0; {}) range", lo.scaleLog2(),
					Consts::NUM_LOD_SCALES);
				return false;
			}

			glm::ivec3 blo = lo.base();
			glm::ivec3 bhi = hi.base();

			if (blo.x >= bhi.x || blo.y >= bhi.y || blo.z >= bhi.z) {
				Log::warn("Bad chunk ticket request: box ({}, {}, {})-({}, {}, {}) is empty or negative", blo.x, blo.y,
					blo.z, bhi.x, bhi.y, bhi.z);
				return false;
			}

			glm::ivec3 diff = glm::abs(bhi - blo);
			int32_t max_dist = std::max({ diff.x, diff.y, diff.z }) >> lo.scale_log2;
			if (max_dist > Consts::MAX_TICKET_BOX_AREA_SIZE) {
				Log::warn("Bad chunk ticket request: box max scaled size {} is larger than maximally allowed {}", max_dist,
					Consts::MAX_TICKET_BOX_AREA_SIZE);
				return false;
			}

			// TODO: validate box coordinates are within world bounds
			return true;
		}

		if (const auto *octa_area = std::get_if<ChunkTicketOctahedronArea>(&area); octa_area != nullptr) {
			ChunkKey pivot = octa_area->pivot;
			assert(pivot.valid());

			if (pivot.scale_log2 >= Consts::NUM_LOD_SCALES) {
				Log::warn("Bad chunk ticket request: LOD {} outside of acceptable [0; {}) range", pivot.scaleLog2(),
					Consts::NUM_LOD_SCALES);
				return false;
			}

			// Ensure scaled radius is non-zero and not too big
			if (octa_area->scaled_radius == 0 || octa_area->scaled_radius > Consts::MAX_TICKET_OCTA_AREA_RADIUS) {
				Log::warn("Bad chunk ticket request: octahedron scaled radius {} outside of acceptable [1; {}] range",
					octa_area->scaled_radius, Consts::MAX_TICKET_OCTA_AREA_RADIUS);
				return false;
			}

			// TODO: validate pivot coordinates are within world bounds
			return true;
		}

		// Is anything else even possible?
		return false;
	}
};

LandService::LandService(svc::ServiceLocator &svc) : m_impl(svc) {}

LandService::~LandService() = default;

void LandService::doTick(WorldTickId tick_id)
{
	m_impl->doTick(tick_id);
}

const LandState &LandService::stateForCopy() const noexcept
{
	return m_impl->landState();
}

} // namespace voxen::land
