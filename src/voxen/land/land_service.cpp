#include <voxen/land/land_service.hpp>

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

LandState::ChunkTable::ValuePtr generateChunk(ChunkKey key)
{
	constexpr int32_t BLOCKS = Consts::CHUNK_SIZE_BLOCKS;

	const glm::ivec3 coords = key.base();
	const glm::ivec3 first_block_coords = coords * BLOCKS;

	// Quickly check if the chunk is fully above/below the surface
	{
		const double ymin = double(first_block_coords.y) * Consts::BLOCK_SIZE_METRES;
		if (ymin > Y_BAND_LIMIT) {
			// Fully above, all zeros
			return {};
		}

		const double ymax = double(first_block_coords.y + (BLOCKS << key.scale_log2)) * Consts::BLOCK_SIZE_METRES;
		if (ymax < -Y_BAND_LIMIT) {
			// Fully below, all ones
			auto ptr = LandState::ChunkTable::makeValuePtr();
			ptr->setAllBlocksUniform(1);
			return ptr;
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
		return {};
	}

	auto ptr = LandState::ChunkTable::makeValuePtr();
	ptr->setAllBlocks(ids->cview());
	return ptr;
}

void loadChunk(ChunkKey key, svc::MessageSender *sender)
{
	assert(key.scale_log2 == 0);

	constexpr int32_t BLOCKS = Consts::CHUNK_SIZE_BLOCKS;

	const glm::ivec3 coords = key.base();
	const glm::ivec3 first_block_coords = coords * BLOCKS;

	// Quickly check if the chunk is fully above/below the surface
	{
		const double ymin = double(first_block_coords.y) * Consts::BLOCK_SIZE_METRES;
		if (ymin > Y_BAND_LIMIT) {
			// Fully above, all zeros
			sender->send<detail::ChunkLoadCompletionMessage>(LandService::SERVICE_UID, key);
			return;
		}

		const double ymax = double(first_block_coords.y + BLOCKS) * Consts::BLOCK_SIZE_METRES;
		if (ymax < -Y_BAND_LIMIT) {
			// Fully below, all ones
			auto ptr = LandState::ChunkTable::makeValuePtr();
			ptr->setAllBlocksUniform(1);
			sender->send<detail::ChunkLoadCompletionMessage>(LandService::SERVICE_UID, key, std::move(ptr));
			return;
		}
	}

	// Allocate on heap, expanded array is pretty large
	auto ids = std::make_unique<Chunk::BlockIdArray>();
	bool empty = true;

	Utils::forYXZ<BLOCKS>([&](uint32_t x, uint32_t y, uint32_t z) {
		const glm::ivec3 pos = glm::ivec3(x, y, z);
		const glm::ivec3 block = first_block_coords + pos;
		const glm::dvec3 block_world = (glm::dvec3(block) + 0.5) * Consts::BLOCK_SIZE_METRES;

		double fn = surfaceFn(block_world);

		if (fn > block_world.y) {
			empty = false;
			(*ids)[pos] = 1;
		} else {
			(*ids)[pos] = 0;
		}
	});

	if (empty) {
		sender->send<detail::ChunkLoadCompletionMessage>(LandService::SERVICE_UID, key);
		return;
	}

	auto ptr = LandState::ChunkTable::makeValuePtr();
	ptr->setAllBlocks(ids->cview());
	sender->send<detail::ChunkLoadCompletionMessage>(LandService::SERVICE_UID, key, std::move(ptr));
}

void generateImpostor(ChunkKey key, std::array<LandState::ChunkTable::ValuePtr, 7> ref, svc::MessageSender *sender)
{
	// Must not be called with empty main chunk
	assert(ref[0]);

	ChunkAdjacencyRef adj(*ref[0]);
	for (size_t i = 0; i < 6; i++) {
		adj.adjacent[i] = ref[i + 1].get();
	}

	auto ptr = LandState::PseudoChunkDataTable::makeValuePtr(adj);

	if (!ptr->empty()) {
		// At least one face exists, then non-empty impostor too
		sender->send<detail::PseudoChunkDataGenCompletionMessage>(LandService::SERVICE_UID, key, std::move(ptr));
	} else {
		// Empty impostor, no need to add it to the tree
		sender->send<detail::PseudoChunkDataGenCompletionMessage>(LandService::SERVICE_UID, key);
	}
}

void generatePseudoSurface(ChunkKey key, std::array<LandState::PseudoChunkDataTable::ValuePtr, 7> ref,
	svc::MessageSender *sender)
{
	// Must not be called with empty main chunk
	assert(ref[0]);

	std::array<const PseudoChunkData *, 6> adjacent;
	for (size_t i = 0; i < 6; i++) {
		adjacent[i] = ref[i + 1].get();
	}

	auto ptr = LandState::PseudoChunkSurfaceTable::makeValuePtr(PseudoChunkSurface::build(*ref[0], adjacent));

	if (!ptr->empty()) {
		// At least one face exists, then non-empty impostor too
		sender->send<detail::PseudoChunkSurfaceGenCompletionMessage>(LandService::SERVICE_UID, key, std::move(ptr));
	} else {
		// Empty impostor, no need to add it to the tree
		sender->send<detail::PseudoChunkSurfaceGenCompletionMessage>(LandService::SERVICE_UID, key);
	}
}

void generatePseudoChunk(ChunkKey key, svc::MessageSender *sender)
{
	std::array<LandState::ChunkTable::ValuePtr, 7> adj;

	adj[0] = generateChunk(key);
	if (!adj[0]) {
		// Empty, return nothing
		sender->send<detail::PseudoChunkDataGenCompletionMessage>(LandService::SERVICE_UID, key);
		return;
	}

	const int32_t step = key.scaleMultiplier();
	adj[1] = generateChunk(ChunkKey(key.x + step, key.y, key.z, key.scale_log2));
	adj[2] = generateChunk(ChunkKey(key.x - step, key.y, key.z, key.scale_log2));
	adj[3] = generateChunk(ChunkKey(key.x, key.y + step, key.z, key.scale_log2));
	adj[4] = generateChunk(ChunkKey(key.x, key.y - step, key.z, key.scale_log2));
	adj[5] = generateChunk(ChunkKey(key.x, key.y, key.z + step, key.scale_log2));
	adj[6] = generateChunk(ChunkKey(key.x, key.y, key.z - step, key.scale_log2));

	generateImpostor(key, std::move(adj), sender);
}

void generateImpostor8(ChunkKey key, std::array<LandState::PseudoChunkDataTable::ValuePtr, 8> lower,
	svc::MessageSender *sender)
{
	const PseudoChunkData *ptrs[8];
	for (size_t i = 0; i < 8; i++) {
		ptrs[i] = lower[i].get();
	}

	sender->send<detail::PseudoChunkDataGenCompletionMessage>(LandService::SERVICE_UID, key,
		LandState::PseudoChunkDataTable::makeValuePtr(ptrs));
}

constexpr int64_t STALE_CHUNK_AGE_THRESHOLD = 750;

struct ChunkMetastate {
	WorldTickId last_referenced_tick = WorldTickId::INVALID;

	uint32_t has_chunk : 1 = 0;
	uint32_t has_fake_data : 1 = 0;
	uint32_t has_pseudo_surface : 1 = 0;
	uint32_t has_pending_chunk_load : 1 = 0;
	uint32_t has_pending_fake_data_gen : 1 = 0;
	uint32_t has_pending_pseudo_surface_gen : 1 = 0;
	uint32_t needs_l0_fake_data : 1 = 0;

	uint64_t internal_ticket = ChunkTicket::INVALID_TICKET_ID;
};

struct TicketState {
	ChunkTicketArea area;
	int32_t priority;
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
		bool logged = false;

		// Jobs can reference this object, wait for completion before destroying
		for (auto &item : m_metastate) {
			if (item.second.has_pending_chunk_load || item.second.has_pending_fake_data_gen) {
				if (std::exchange(logged, true) == false) {
					Log::debug("Have pending jobs remaining, waiting...");
				}

				m_queue.waitMessages();
			}
		}
	}

	void doTick(WorldTickId tick_id)
	{
		m_tick_id = tick_id;

		// Process chunk ticket change requests, now we have a fresh list of tickets
		m_queue.pollMessages();

		// No keys left to update for this tick, collect a new list.
		// It might be very big if there are many tickets (e.g. loading lots of terrain at startup)
		// but we will consume it in batches over the following ticks.
		if (m_keys_to_update.empty()) {
			for (const auto &[id, state] : m_chunk_tickets) {
				if (const auto *box_area = std::get_if<ChunkTicketBoxArea>(&state.area); box_area != nullptr) {
					const ChunkKey lo = box_area->begin;
					const ChunkKey hi = box_area->end;
					const int32_t step = lo.scaleMultiplier();

					for (int64_t y = lo.y; y < hi.y; y += step) {
						for (int64_t x = lo.x; x < hi.x; x += step) {
							for (int64_t z = lo.z; z < hi.z; z += step) {
								ChunkKey ck(x, y, z, lo.scale_log2);
								m_keys_to_update.emplace_back(state.priority, ck);
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
						m_keys_to_update.emplace_back(state.priority, ck);
					}
				}
			}

			// First eliminate duplicate keys from overlapping tickets,
			// leaving only the ones with the highest priority.
			// First sort by comparing (key, priority) instead of (priority, key).
			std::sort(m_keys_to_update.begin(), m_keys_to_update.end(),
				[](const auto &a, const auto &b) { return std::tie(a.second, a.first) < std::tie(b.second, b.first); });
			// Now all entries with the same key are grouped together and ordered by priority value.
			// Leave only the first of every group - i.e. that with highest priority.
			auto last = std::unique(m_keys_to_update.begin(), m_keys_to_update.end(),
				[](const auto &a, const auto &b) { return a.second == b.second; });
			m_keys_to_update.erase(last, m_keys_to_update.end());
			// Now sort by comparing (priority, key) so that high-priority keys are visited first.
			// Note that we are sorting in reverse - we will do pop_back to quickly remove visited keys.
			std::sort(m_keys_to_update.rbegin(), m_keys_to_update.rend());
		}

		// Limit the number of keys visited per tick.
		// TODO: move to constants/options/auto-adjust?
		constexpr size_t KEYS_PER_TICK = SIZE_MAX; //5500;
		const size_t num_visited = std::min(KEYS_PER_TICK, m_keys_to_update.size());

		for (size_t i = 0; i < num_visited; i++) {
			tickChunkKey(m_keys_to_update.back().second, tick_id, m_keys_to_update.back().first);
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

				if (!canCleanupChunk(iter->second)) {
					// Has some pending work, unsafe to remove.
					// This will leave it pretty much at the same place - the chunk
					// itself is stale, we just need to wait for jobs completion.
					return tick_id + 1;
				}

				uint64_t version = static_cast<uint64_t>(tick_id.value);
				m_land_state.chunk_table.erase(version, iter->first);
				m_land_state.pseudo_chunk_data_table.erase(version, iter->first);
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

	std::unordered_map<uint64_t, TicketState> m_chunk_tickets;
	uint64_t m_next_ticket_id = 0;

	std::unordered_map<ChunkKey, ChunkMetastate> m_metastate;

	LruVisitOrdering<ChunkKey, WorldTickTag> m_keys_lru_check_order;
	std::vector<std::pair<int32_t, ChunkKey>> m_keys_to_update;

	WorldTickId m_tick_id;
	LandState m_land_state;

	void tickChunkKey(ChunkKey ck, WorldTickId tick_id, int32_t priority)
	{
		auto [iter, inserted] = m_metastate.try_emplace(ck);
		if (inserted) {
			// Register this key in cleanup visit ordering
			m_keys_lru_check_order.addKey(ck, tick_id + STALE_CHUNK_AGE_THRESHOLD);
		}

		auto &m = iter->second;
		m.last_referenced_tick = tick_id;

		if (ck.scale_log2 == 0 && !m.has_chunk && !m.has_pending_chunk_load) {
			// No chunk and no loading job - enqueue one
			m.has_pending_chunk_load = 1;
			svc::TaskBuilder(m_task_service).enqueueTask([ck, snd = &m_sender](svc::TaskContext &) {
				loadChunk(ck, snd);
			});
		}

		if (!m.has_fake_data && !m.has_pending_fake_data_gen) {
			// No fake data and no gen job - enqueue one if needed
			tryChunkFakeDataGen(ck, m, priority);
		}

		if (!m.has_pseudo_surface && !m.has_pending_pseudo_surface_gen) {
			// No pseudo surface and no gen job - enqueue one if needed
			tryPseudoSurfaceGen(ck, m, priority);
		}
	}

	static bool canCleanupChunk(ChunkMetastate &m)
	{
		if (m.has_pending_chunk_load || m.has_pending_fake_data_gen || m.has_pending_pseudo_surface_gen) {
			// Has pending job, unsafe to remove now
			return false;
		}

		// Not referenced for some time, no pending jobs - safe to remove
		return true;
	}

	void tryChunkFakeDataGen(ChunkKey ck, ChunkMetastate &m, int32_t priority)
	{
		if (ck.scale_log2 == 0) {
			if (!m.needs_l0_fake_data && priority > 0) {
				// Avoid infinite "adjacency creep"
				return;
			}

			if (!m.has_chunk) {
				// Wait until at least our chunk loads (don't need ticket for that)
				return;
			}

			bool all_found = true;
			bool have_nonempty = false;
			std::array<LandState::ChunkTable::ValuePtr, 7> dependencies;

			auto *my_chunk_item = m_land_state.chunk_table.find(ck);
			assert(my_chunk_item != nullptr);
			if (!my_chunk_item->hasValue()) {
				// Empty chunk, "generate" impostor immediately
				m.has_fake_data = 1;
				m_land_state.pseudo_chunk_data_table.insert(static_cast<uint64_t>(m_tick_id.value), ck, {});
				return;
			} else {
				dependencies[0] = my_chunk_item->valuePtr();
			}

			auto find_dependency = [&](ChunkKey dk, size_t index) {
				const auto *item = m_land_state.chunk_table.find(dk);
				if (!item) {
					all_found = false;
				} else {
					dependencies[index] = item->valuePtr();
					have_nonempty |= item->hasValue();
				}
			};

			const glm::ivec3 B = ck.base();

			find_dependency(ChunkKey(B + glm::ivec3(1, 0, 0)), 1);
			find_dependency(ChunkKey(B - glm::ivec3(1, 0, 0)), 2);
			find_dependency(ChunkKey(B + glm::ivec3(0, 1, 0)), 3);
			find_dependency(ChunkKey(B - glm::ivec3(0, 1, 0)), 4);
			find_dependency(ChunkKey(B + glm::ivec3(0, 0, 1)), 5);
			find_dependency(ChunkKey(B - glm::ivec3(0, 0, 1)), 6);

			if (all_found) {
				if (!have_nonempty) {
					// All adjacent chunks are empty, "generate" impostor immediately
					m.has_fake_data = 1;
					m_land_state.pseudo_chunk_data_table.insert(static_cast<uint64_t>(m_tick_id.value), ck, {});
					return;
				}

				// Have all dependencies, enqueue task immediately, no ticket needed
				m.has_pending_fake_data_gen = 1;
				svc::TaskBuilder(m_task_service)
					.enqueueTask([ck, deps = std::move(dependencies), snd = &m_sender](svc::TaskContext &) {
						return generateImpostor(ck, std::move(deps), snd);
					});
				return;
			}

			if (m.internal_ticket == ChunkTicket::INVALID_TICKET_ID) {
				// Take a ticket covering the needed adjacent chunks.
				// Need 6 adjacent chunks in cross (+1 our own).
				m.internal_ticket = addInternalTicket(
					ChunkTicketOctahedronArea {
						.pivot = ck,
						.scaled_radius = 1,
					},
					priority + 1);
			}

			return;
		}

		if (ck.scale_log2 <= Consts::MAX_DIRECT_GENERATE_LOD) {
			// Small LOD, generate it directly
			m.has_pending_fake_data_gen = 1;
			svc::TaskBuilder(m_task_service).enqueueTask([ck, snd = &m_sender](svc::TaskContext &) {
				return generatePseudoChunk(ck, snd);
			});
			return;
		}

		// LOD not zero, separate path for this
		bool all_found = true;
		bool have_nonempty = false;
		std::array<LandState::PseudoChunkDataTable::ValuePtr, 8> dependencies;

		auto find_dependency = [&](ChunkKey dk, size_t index) {
			const auto *item = m_land_state.pseudo_chunk_data_table.find(dk);

			if (!item) {
				all_found = false;

				if (dk.scale_log2 == 0) {
					// We need this chunk to generate its L0 fake data to aggregate it into L1
					m_metastate[dk].needs_l0_fake_data = 1;
				}
			} else {
				dependencies[index] = item->valuePtr();
				have_nonempty |= item->hasValue();
			}
		};

		const glm::ivec3 B = ck.base();
		const uint32_t S = ck.scale_log2 - 1u;
		const int32_t K = ck.scaleMultiplier() / 2;

		find_dependency(ChunkKey(B + glm::ivec3(0, 0, K), S), 0);
		find_dependency(ChunkKey(B + glm::ivec3(K, 0, K), S), 1);
		find_dependency(ChunkKey(B + glm::ivec3(0, K, K), S), 2);
		find_dependency(ChunkKey(B + glm::ivec3(K, K, K), S), 3);
		find_dependency(ChunkKey(B + glm::ivec3(0, 0, 0), S), 4);
		find_dependency(ChunkKey(B + glm::ivec3(K, 0, 0), S), 5);
		find_dependency(ChunkKey(B + glm::ivec3(0, K, 0), S), 6);
		find_dependency(ChunkKey(B + glm::ivec3(K, K, 0), S), 7);

		if (all_found) {
			if (!have_nonempty) {
				// All dependencies are empty, "generate" impostor immediately
				m.has_fake_data = 1;
				m_land_state.pseudo_chunk_data_table.insert(static_cast<uint64_t>(m_tick_id.value), ck, {});
				return;
			}

			// Have all dependencies, enqueue task immediately, no ticket needed
			m.has_pending_fake_data_gen = 1;
			svc::TaskBuilder(m_task_service)
				.enqueueTask([ck, deps = std::move(dependencies), snd = &m_sender](svc::TaskContext &) {
					return generateImpostor8(ck, std::move(deps), snd);
				});
			return;
		}

		if (m.internal_ticket == ChunkTicket::INVALID_TICKET_ID) {
			// Take a ticket covering the needed adjacent chunks.
			// Need 8 (2x2x2) "child LOD cube" chunks.
			m.internal_ticket = addInternalTicket(
				ChunkTicketBoxArea {
					.begin = ChunkKey(ck.base(), ck.scale_log2 - 1),
					.end = ChunkKey(ck.base() + ck.scaleMultiplier(), ck.scale_log2 - 1),
				},
				priority + 5);
		}
	}

	void tryPseudoSurfaceGen(ChunkKey ck, ChunkMetastate &m, int32_t priority)
	{
		if (!m.has_fake_data || m.has_pending_fake_data_gen) {
			// Wait for at least our pseudo data to generate
			return;
		}

		bool all_found = true;
		bool have_nonempty = false;
		std::array<LandState::PseudoChunkDataTable::ValuePtr, 7> dependencies;

		auto *my_chunk_item = m_land_state.pseudo_chunk_data_table.find(ck);
		assert(my_chunk_item != nullptr);
		if (!my_chunk_item->hasValue()) {
			// Empty chunk, "generate" its pseudo surface immediately
			m.has_fake_data = 1;
			m_land_state.pseudo_chunk_surface_table.insert(static_cast<uint64_t>(m_tick_id.value), ck, {});
			return;
		} else {
			dependencies[0] = my_chunk_item->valuePtr();
		}

		auto find_dependency = [&](ChunkKey dk, size_t index) {
			const auto *item = m_land_state.pseudo_chunk_data_table.find(dk);
			if (!item) {
				all_found = false;
			} else {
				dependencies[index] = item->valuePtr();
				have_nonempty |= item->hasValue();
			}
		};

		const glm::ivec3 B = ck.base();

		find_dependency(ChunkKey(B + glm::ivec3(1, 0, 0)), 1);
		find_dependency(ChunkKey(B - glm::ivec3(1, 0, 0)), 2);
		find_dependency(ChunkKey(B + glm::ivec3(0, 1, 0)), 3);
		find_dependency(ChunkKey(B - glm::ivec3(0, 1, 0)), 4);
		find_dependency(ChunkKey(B + glm::ivec3(0, 0, 1)), 5);
		find_dependency(ChunkKey(B - glm::ivec3(0, 0, 1)), 6);

		if (all_found) {
			if (!have_nonempty) {
				// All adjacent chunks are empty, "generate" pseudo surface immediately
				m.has_fake_data = 1;
				m_land_state.pseudo_chunk_surface_table.insert(static_cast<uint64_t>(m_tick_id.value), ck, {});
				return;
			}

			// Have all dependencies, enqueue task immediately, no ticket needed
			m.has_pending_pseudo_surface_gen = 1;
			svc::TaskBuilder(m_task_service)
				.enqueueTask([ck, deps = std::move(dependencies), snd = &m_sender](svc::TaskContext &) {
					return generatePseudoSurface(ck, std::move(deps), snd);
				});
			return;
		}

		if (m.internal_ticket == ChunkTicket::INVALID_TICKET_ID) {
			// Take a ticket covering the needed adjacent chunks.
			// Need 6 adjacent chunks in cross (+1 our own).
			m.internal_ticket = addInternalTicket(
				ChunkTicketOctahedronArea {
					.pivot = ck,
					.scaled_radius = 1,
				},
				priority + 1);
		}
	}

	static int32_t ticketAreaPriority(const ChunkTicketArea &area) noexcept
	{
		if (const auto *box_area = std::get_if<ChunkTicketBoxArea>(&area); box_area != nullptr) {
			return box_area->begin.scale_log2;
		}

		return std::get_if<ChunkTicketOctahedronArea>(&area)->pivot.scale_log2;
	}

	void handleChunkTicketRequest(ChunkTicketRequestMessage &msg, svc::MessageInfo &info)
	{
		if (!validateChunkTicketArea(msg.area)) {
			Log::warn("Bad chunk ticket request came from {}; returning null handle",
				debug::UidRegistry::lookup(info.senderUid()));
			msg.ticket = {};
			return;
		}

		uint64_t ticket_id = addInternalTicket(msg.area, ticketAreaPriority(msg.area));
		msg.ticket = ChunkTicket(ticket_id, &m_sender);
	}

	void handleChunkTicketAdjust(const ChunkTicketAdjustMessage &msg)
	{
		if (!validateChunkTicketArea(msg.new_area)) {
			// Sender is unknown - this message comes from our special ticket sender
			Log::warn("Bad chunk ticket adjust request for ticket ID {}; ticket not changed", msg.ticket_id);
			return;
		}

		uint64_t ticket_id = msg.ticket_id;
		auto iter = m_chunk_tickets.find(ticket_id);
		assert(iter != m_chunk_tickets.end());
		iter->second.area = msg.new_area;
		iter->second.priority = ticketAreaPriority(msg.new_area);
	}

	void handleChunkTicketRemove(const ChunkTicketRemoveMessage &msg) { removeInternalTicket(msg.ticket_id); }

	void handleChunkLoadCompletion(ChunkLoadCompletionMessage &msg)
	{
		auto &m = m_metastate[msg.key];
		assert(m.has_pending_chunk_load);
		m.has_chunk = 1;
		m.has_pending_chunk_load = 0;
		m_land_state.chunk_table.insert(static_cast<uint64_t>(m_tick_id.value), msg.key, std::move(msg.value_ptr));
	}

	void handlePseudoDataGenCompletion(PseudoChunkDataGenCompletionMessage &msg)
	{
		auto &m = m_metastate[msg.key];
		assert(m.has_pending_fake_data_gen);
		m.has_fake_data = 1;
		m.has_pending_fake_data_gen = 0;

		if (msg.key.scale_log2 == 0) {
			// Don't regenerate fake data every time, an adjacent chunk needing it will set the flag again
			m.needs_l0_fake_data = 0;
		}

		uint64_t ticket_id = std::exchange(m.internal_ticket, ChunkTicket::INVALID_TICKET_ID);
		if (ticket_id != ChunkTicket::INVALID_TICKET_ID) {
			removeInternalTicket(ticket_id);
		}

		m_land_state.pseudo_chunk_data_table.insert(static_cast<uint64_t>(m_tick_id.value), msg.key,
			std::move(msg.value_ptr));
	}

	void handlePseudoSurfaceGenCompletion(PseudoChunkSurfaceGenCompletionMessage &msg)
	{
		auto &m = m_metastate[msg.key];
		assert(m.has_pending_pseudo_surface_gen);
		m.has_pseudo_surface = 1;
		m.has_pending_pseudo_surface_gen = 0;

		uint64_t ticket_id = std::exchange(m.internal_ticket, ChunkTicket::INVALID_TICKET_ID);
		if (ticket_id != ChunkTicket::INVALID_TICKET_ID) {
			removeInternalTicket(ticket_id);
		}

		m_land_state.pseudo_chunk_surface_table.insert(static_cast<uint64_t>(m_tick_id.value), msg.key,
			std::move(msg.value_ptr));
	}

	uint64_t addInternalTicket(const ChunkTicketArea &area, int32_t priority)
	{
		TicketState state {
			.area = area,
			.priority = priority,
		};

		uint64_t ticket_id = m_next_ticket_id++;
		m_chunk_tickets.emplace(ticket_id, state);
		return ticket_id;
	}

	void removeInternalTicket(uint64_t ticket_id)
	{
		assert(m_chunk_tickets.contains(ticket_id));
		m_chunk_tickets.erase(ticket_id);
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
