#include <voxen/common/land/land.hpp>

#include <voxen/common/land/block_registry.hpp>
#include <voxen/common/land/land_utils.hpp>
#include <voxen/common/threadpool.hpp>
#include <voxen/common/v8g_flat_map_impl.hpp>
#include <voxen/common/v8g_hash_trie_impl.hpp>
#include <voxen/util/concentric_octahedron_walker.hpp>
#include <voxen/util/log.hpp>

#include "land_private_consts.hpp"

#include <list>

namespace voxen
{

// Instantiate templates in this translation unit
template class V8gHashTrie<land::ChunkKey, land::Chunk>;
template class V8gHashTrie<land::ChunkKey, land::FakeChunkData>;

} // namespace voxen

namespace voxen::land
{

namespace
{

LandState::ChunkTable::ValuePtr loadChunk(ChunkKey key)
{
	assert(key.scale_log2 == 0);

	constexpr int32_t BLOCKS = Consts::CHUNK_SIZE_BLOCKS;

	const glm::ivec3 coords = key.base();
	const glm::ivec3 first_block_coords = coords * BLOCKS;

	// Quickly check if the chunk is fully above/below the surface
	{
		constexpr double Y_BAND_LIMIT = 90.0;

		const double ymin = double(first_block_coords.y) * Consts::BLOCK_SIZE_METRES;
		if (ymin > Y_BAND_LIMIT) {
			// Fully above, all zeros
			return {};
		}

		const double ymax = double(first_block_coords.y + BLOCKS) * Consts::BLOCK_SIZE_METRES;
		if (ymax < -Y_BAND_LIMIT) {
			// Fully below, all ones
			auto ptr = LandState::ChunkTable::makeValuePtr();
			ptr->setAllBlocksUniform(1);
			return ptr;
		}
	}

	// Allocate on heap, expanded array is pretty large
	auto ids = std::make_unique<Chunk::BlockIdStorage::ExpandedArray>();
	bool empty = true;

	constexpr std::tuple<double, double, double, double> octaves[] = {
		{ 4.0, 0.5, 0.03, 0.09 },
		{ 8.0, 3.5, -0.013, 0.048 },
		{ 16.0, 14.1, 0.0095, -0.0205 },
		{ 12.0, -7.5, -0.08, 0.0333 },
		{ 64.0, 7.65, 0.007, 0.0032 },
	};

	for (int32_t y = 0; y < BLOCKS; y++) {
		for (int32_t x = 0; x < BLOCKS; x++) {
			for (int32_t z = 0; z < BLOCKS; z++) {
				const glm::ivec3 pos = glm::ivec3(x, y, z);
				const glm::ivec3 block = first_block_coords + pos;
				const glm::dvec3 block_world = (glm::dvec3(block) + 0.5) * Consts::BLOCK_SIZE_METRES;

				double fn = 0.0;
				for (const auto &[amp, phi, fx, fz] : octaves) {
					fn += amp * sin(phi + block_world.x * fx + block_world.z * fz);
				}

				//fn *= 0.000001;

				//fn += 40.0;
				//fn += 12.0;

				if (fn > block_world.y) {
					empty = false;
					(*ids)[pos] = 1;
				} else {
					(*ids)[pos] = 0;
				}
			}
		}
	}

	if (empty) {
		return {};
	}

	auto ptr = LandState::ChunkTable::makeValuePtr();
	ptr->setAllBlocks(*ids);
	return ptr;
}

LandState::FakeChunkDataTable::ValuePtr generateImpostor(std::array<LandState::ChunkTable::ValuePtr, 7> ref)
{
	if (!ref[0]) {
		return {};
	}

	ChunkAdjacencyRef adj(*ref[0]);
	for (size_t i = 0; i < 6; i++) {
		adj.adjacent[i] = ref[i + 1].get();
	}

	auto ptr = LandState::FakeChunkDataTable::makeValuePtr(adj);

	if (!ptr->empty()) {
		// At least one face exists, then non-empty impostor too
		return ptr;
	}

	// Empty impostor, no need to add it to the tree
	return {};
}

LandState::FakeChunkDataTable::ValuePtr generateImpostor8(std::array<LandState::FakeChunkDataTable::ValuePtr, 8> lower)
{
	if (std::all_of(lower.begin(), lower.end(), [](const auto &ptr) { return !ptr; })) {
		return {};
	}

	const FakeChunkData *ptrs[8];
	for (size_t i = 0; i < 8; i++) {
		ptrs[i] = lower[i].get();
	}

	return LandState::FakeChunkDataTable::makeValuePtr(ptrs);
}

struct ChunkLoadTicket {
	glm::ivec3 pivot;
	int32_t distance;
};

using ChunkLoadTicketPtr = std::shared_ptr<ChunkLoadTicket>;

struct ImpostorGenTicket {
	glm::ivec3 scaled_pivot;
	int32_t scaled_distance;
	uint32_t scale_log2;
};

using ImpostorGenTicketPtr = std::shared_ptr<ImpostorGenTicket>;

struct KeyMetastate {
	int32_t chunk_ticket_refs = 0;
	int32_t impostor_ticket_refs = 0;
	int32_t pending_dependencies = 0;

	bool has_chunk_load_pending = false;
	bool has_impostor_gen_pending = false;

	ChunkLoadTicketPtr adjacent_chunk_load_ticket;
	ImpostorGenTicketPtr lower_impostor_gen_ticket[8];

	std::vector<ChunkKey> chunk_load_dependent_impostor_gens;
	std::vector<ChunkKey> impostor_gen_dependent_impostor_gens;

	bool safeToDelete() const noexcept
	{
		return chunk_ticket_refs == 0 && impostor_ticket_refs == 0 && pending_dependencies == 0 && !has_chunk_load_pending
			&& !has_impostor_gen_pending;
	}
};

class TestBlock final : public IBlock {
public:
	~TestBlock() override = default;

	std::string_view getInternalName() const noexcept override { return "voxen/test_block"; }

	PackedColorLinear getImpostorColor() const noexcept override { return PackedColorLinear(0x02, 0x2F, 0x8E); }
};

} // namespace

struct Land::Impl {
	uint64_t timeline = 0;

	glm::dvec3 loading_point = {};

	ChunkLoadTicketPtr loading_ticket = {};
	ImpostorGenTicketPtr l0_gen_ticket = {};
	ImpostorGenTicketPtr l1_gen_ticket = {};
	ImpostorGenTicketPtr l2_gen_ticket = {};
	ImpostorGenTicketPtr l3_gen_ticket = {};

	std::unordered_map<ChunkKey, KeyMetastate> key_metastate;
	std::vector<std::pair<ChunkKey, std::future<LandState::ChunkTable::ValuePtr>>> chunk_loading_requests;
	std::vector<std::pair<ChunkKey, std::future<LandState::FakeChunkDataTable::ValuePtr>>> impostor_gen_requests;

	std::deque<ChunkKey> chunk_load_tasks;
	std::deque<ChunkKey> impostor_gen_tasks;

	std::list<std::pair<glm::ivec3, ConcentricOctahedronWalker>> chunk_ticket_dec_walkers;
	std::list<std::tuple<glm::ivec3, uint32_t, ConcentricOctahedronWalker>> impostor_gen_dec_walkers;

	BlockRegistry block_registry;

	LandState state;

	void unblockChunkLoadDependents(ChunkKey key)
	{
		auto &metastate = key_metastate[key];
		metastate.has_chunk_load_pending = false;

		for (ChunkKey dep_key : metastate.chunk_load_dependent_impostor_gens) {
			if (--key_metastate[dep_key].pending_dependencies == 0) {
				// Schedule it finally

				std::array<LandState::ChunkTable::ValuePtr, 7> dependencies;

				auto find_dependency = [&](ChunkKey dk, size_t index) {
					const auto *item = state.chunk_table.find(dk);
					assert(item);
					dependencies[index] = item->valuePtr();
				};

				const glm::ivec3 B = dep_key.base();

				find_dependency(dep_key, 0);
				find_dependency(ChunkKey(B + glm::ivec3(1, 0, 0)), 1);
				find_dependency(ChunkKey(B - glm::ivec3(1, 0, 0)), 2);
				find_dependency(ChunkKey(B + glm::ivec3(0, 1, 0)), 3);
				find_dependency(ChunkKey(B - glm::ivec3(0, 1, 0)), 4);
				find_dependency(ChunkKey(B + glm::ivec3(0, 0, 1)), 5);
				find_dependency(ChunkKey(B - glm::ivec3(0, 0, 1)), 6);

				auto &pool = ThreadPool::globalVoxenPool();
				auto future = pool.enqueueTask(ThreadPool::TaskType::Standard, generateImpostor, std::move(dependencies));
				impostor_gen_requests.emplace_back(dep_key, std::move(future));
			}
		}
		metastate.chunk_load_dependent_impostor_gens.clear();

		if (metastate.chunk_ticket_refs == 0) {
			state.chunk_table.erase(timeline, key);
		}

		if (metastate.safeToDelete()) {
			key_metastate.erase(key);
		}
	}

	void unblockImpostorGenDependents(ChunkKey key)
	{
		auto &metastate = key_metastate[key];
		metastate.has_impostor_gen_pending = false;

		metastate.adjacent_chunk_load_ticket.reset();
		for (auto &tkt : metastate.lower_impostor_gen_ticket) {
			tkt.reset();
		}

		for (ChunkKey dep_key : metastate.impostor_gen_dependent_impostor_gens) {
			if (--key_metastate[dep_key].pending_dependencies == 0) {
				// Schedule it finally

				std::array<LandState::FakeChunkDataTable::ValuePtr, 8> dependencies;

				auto find_dependency = [&](ChunkKey dk, size_t index) {
					const auto *item = state.fake_chunk_data_table.find(dk);

					if (!item) {
						auto &dep_metastate = key_metastate[dep_key];
						auto &dk_metastate = key_metastate[dk];

						glm::ivec3 dkb = dep_key.base();
						Log::fatal("CONTRACT FAILED: MAKING IMPOSTOR FOR KEY {}|{}|{} (L{})", dkb.x, dkb.y, dkb.z,
							dep_key.scaleLog2());
						dkb = dk.base();
						Log::fatal("MISSING REQUIRED DEPENDENCY {}|{}|{} (L{})", dkb.x, dkb.y, dkb.z, dk.scaleLog2());
						Log::fatal("ITS METASTATE: HIGP {}, ITR {}, IGIGDs {}", dk_metastate.has_impostor_gen_pending,
							dk_metastate.impostor_ticket_refs, dk_metastate.impostor_gen_dependent_impostor_gens.size());

						for (int i = 0; i < 8; i++) {
							const auto &tkt = dep_metastate.lower_impostor_gen_ticket[i];
							if (!tkt) {
								Log::fatal("TICKET #{} => MISSING!!!", i);
							} else {
								dkb = tkt->scaled_pivot;
								glm::ivec3 dkbs = dkb << int32_t(tkt->scale_log2);
								Log::fatal("TICKET #{} => {}|{}|{} (L{}) +{} => {}|{}|{} +{}", i, dkb.x, dkb.y, dkb.z,
									tkt->scale_log2, tkt->scaled_distance, dkbs.x, dkbs.y, dkbs.z,
									tkt->scaled_distance << tkt->scale_log2);
							}
						}

						Log::fatal("ABORTING EXECUTION");
						abort();
					}

					assert(item);
					dependencies[index] = item->valuePtr();
				};

				const glm::ivec3 B = dep_key.base();
				const uint32_t S = dep_key.scale_log2 - 1u;
				const int32_t K = dep_key.scaleMultiplier() / 2;

				find_dependency(ChunkKey(B + glm::ivec3(0, 0, K), S), 0);
				find_dependency(ChunkKey(B + glm::ivec3(K, 0, K), S), 1);
				find_dependency(ChunkKey(B + glm::ivec3(0, K, K), S), 2);
				find_dependency(ChunkKey(B + glm::ivec3(K, K, K), S), 3);
				find_dependency(ChunkKey(B + glm::ivec3(0, 0, 0), S), 4);
				find_dependency(ChunkKey(B + glm::ivec3(K, 0, 0), S), 5);
				find_dependency(ChunkKey(B + glm::ivec3(0, K, 0), S), 6);
				find_dependency(ChunkKey(B + glm::ivec3(K, K, 0), S), 7);

				auto &pool = ThreadPool::globalVoxenPool();
				auto future = pool.enqueueTask(ThreadPool::TaskType::Standard, generateImpostor8, std::move(dependencies));
				impostor_gen_requests.emplace_back(dep_key, std::move(future));
			}
		}
		metastate.impostor_gen_dependent_impostor_gens.clear();

		if (metastate.impostor_ticket_refs == 0) {
			state.fake_chunk_data_table.erase(timeline, key);
		}

		if (metastate.safeToDelete()) {
			key_metastate.erase(key);
		}
	}

	void processRequestsCompletion()
	{
		size_t remains_checked = Consts::MAX_CHECKED_COMPLETIONS_PER_TICK;
		size_t remains_processed = Consts::MAX_PROCESSED_COMPLETIONS_PER_TICK;

		for (auto iter = chunk_loading_requests.begin(); iter != chunk_loading_requests.end(); /*nothing*/) {
			if (iter->second.wait_for(std::chrono::nanoseconds(0)) == std::future_status::ready) {
				state.chunk_table.insert(timeline, iter->first, iter->second.get());
				unblockChunkLoadDependents(iter->first);
				iter = chunk_loading_requests.erase(iter);

				if (--remains_processed == 0) {
					break;
				}
			} else {
				++iter;
			}

			if (--remains_checked == 0) {
				break;
			}
		}

		// Reset counters - make processing fair for all kinds of requests
		remains_checked = Consts::MAX_CHECKED_COMPLETIONS_PER_TICK;
		remains_processed = Consts::MAX_PROCESSED_COMPLETIONS_PER_TICK;

		for (auto iter = impostor_gen_requests.begin(); iter != impostor_gen_requests.end(); /*nothing*/) {
			if (iter->second.wait_for(std::chrono::nanoseconds(0)) == std::future_status::ready) {
				state.fake_chunk_data_table.insert(timeline, iter->first, iter->second.get());

				unblockImpostorGenDependents(iter->first);
				iter = impostor_gen_requests.erase(iter);

				if (--remains_processed == 0) {
					break;
				}
			} else {
				++iter;
			}

			if (--remains_checked == 0) {
				break;
			}
		}
	}

	void processSingleChunkLoading(ChunkKey key)
	{
		auto &metastate = key_metastate[key];

		if (metastate.has_chunk_load_pending) {
			return;
		}

		if (state.chunk_table.find(key) != nullptr) {
			return;
		}

		// Chunk is neither loaded nor requested for, add a job for it
		metastate.has_chunk_load_pending = true;

		auto &pool = ThreadPool::globalVoxenPool();
		auto future = pool.enqueueTask(ThreadPool::TaskType::Standard, loadChunk, key);
		chunk_loading_requests.emplace_back(key, std::move(future));
	}

	void processSingleImpostorGen(ChunkKey key)
	{
		auto &metastate = key_metastate[key];

		if (metastate.has_impostor_gen_pending) {
			return;
		}

		if (state.fake_chunk_data_table.find(key) != nullptr) {
			return;
		}

		// Impostor is neither generated nor requested for, add a job for it
		metastate.has_impostor_gen_pending = true;

		glm::ivec3 pivot = key.base();

		if (key.scale_log2 > 0) {
			// Find dependencies
			bool have_all_dependencies = true;
			std::array<LandState::FakeChunkDataTable::ValuePtr, 8> dependencies;

			auto check_dependency = [&](ChunkKey dep_key, size_t index) {
				// Ticket adjacent impostors so they won't spontaneously unload
				metastate.lower_impostor_gen_ticket[index] = addImpostorGenTicket(ImpostorGenTicket {
					.scaled_pivot = dep_key.base() >> int32_t(dep_key.scale_log2),
					.scaled_distance = 0,
					.scale_log2 = dep_key.scaleLog2(),
				});

				auto &dep_metastate = key_metastate[dep_key];
				const auto *item = state.fake_chunk_data_table.find(dep_key);

				if (dep_metastate.has_impostor_gen_pending || item == nullptr) {
					have_all_dependencies = false;
					dep_metastate.impostor_gen_dependent_impostor_gens.emplace_back(key);
					metastate.pending_dependencies++;
				} else {
					dependencies[index] = item->valuePtr();
				}
			};

			const uint32_t S = key.scale_log2 - 1u;
			const int32_t K = key.scaleMultiplier() / 2;

			check_dependency(ChunkKey(pivot + glm::ivec3(0, 0, K), S), 0);
			check_dependency(ChunkKey(pivot + glm::ivec3(K, 0, K), S), 1);
			check_dependency(ChunkKey(pivot + glm::ivec3(0, K, K), S), 2);
			check_dependency(ChunkKey(pivot + glm::ivec3(K, K, K), S), 3);
			check_dependency(ChunkKey(pivot + glm::ivec3(0, 0, 0), S), 4);
			check_dependency(ChunkKey(pivot + glm::ivec3(K, 0, 0), S), 5);
			check_dependency(ChunkKey(pivot + glm::ivec3(0, K, 0), S), 6);
			check_dependency(ChunkKey(pivot + glm::ivec3(K, K, 0), S), 7);

			if (have_all_dependencies) {
				auto &pool = ThreadPool::globalVoxenPool();
				auto future = pool.enqueueTask(ThreadPool::TaskType::Standard, generateImpostor8, std::move(dependencies));
				impostor_gen_requests.emplace_back(key, std::move(future));
			}

			return;
		}

		// Ticket adjacent chunks so they won't spontaneously unload
		metastate.adjacent_chunk_load_ticket = addChunkLoadTicket(ChunkLoadTicket {
			.pivot = pivot,
			.distance = 1,
		});

		// Find dependencies
		bool have_all_dependencies = true;
		std::array<LandState::ChunkTable::ValuePtr, 7> dependencies;

		auto check_dependency = [&](ChunkKey dep_key, size_t index) {
			auto &dep_metastate = key_metastate[dep_key];
			const auto *item = state.chunk_table.find(dep_key);

			if (dep_metastate.has_chunk_load_pending || item == nullptr) {
				have_all_dependencies = false;
				dep_metastate.chunk_load_dependent_impostor_gens.emplace_back(key);
				metastate.pending_dependencies++;
			} else {
				dependencies[index] = item->valuePtr();
			}
		};

		check_dependency(key, 0);
		check_dependency(ChunkKey(pivot + glm::ivec3(1, 0, 0)), 1);
		check_dependency(ChunkKey(pivot - glm::ivec3(1, 0, 0)), 2);
		check_dependency(ChunkKey(pivot + glm::ivec3(0, 1, 0)), 3);
		check_dependency(ChunkKey(pivot - glm::ivec3(0, 1, 0)), 4);
		check_dependency(ChunkKey(pivot + glm::ivec3(0, 0, 1)), 5);
		check_dependency(ChunkKey(pivot - glm::ivec3(0, 0, 1)), 6);

		if (have_all_dependencies) {
			auto &pool = ThreadPool::globalVoxenPool();
			auto future = pool.enqueueTask(ThreadPool::TaskType::Standard, generateImpostor, std::move(dependencies));
			impostor_gen_requests.emplace_back(key, std::move(future));
		}
	}

	void processChunkLoading()
	{
		if (chunk_loading_requests.size() > 10000 || impostor_gen_requests.size() > 10000) {
			return;
		}

		glm::ivec3 new_pivot_chunk = glm::ivec3(glm::round(loading_point / Consts::CHUNK_SIZE_METRES));

		if (!loading_ticket || loading_ticket->pivot != new_pivot_chunk) {
			loading_ticket = addChunkLoadTicket(ChunkLoadTicket {
				.pivot = new_pivot_chunk,
				.distance = Consts::CHUNK_LOAD_DISTANCE_CHUNKS,
			});

			l0_gen_ticket = addImpostorGenTicket(ImpostorGenTicket {
				.scaled_pivot = new_pivot_chunk,
				.scaled_distance = Consts::CHUNK_LOAD_DISTANCE_CHUNKS,
				.scale_log2 = 0,
			});

			l1_gen_ticket = addImpostorGenTicket(ImpostorGenTicket {
				.scaled_pivot = new_pivot_chunk / 2,
				.scaled_distance = Consts::CHUNK_LOAD_DISTANCE_CHUNKS,
				.scale_log2 = 1,
			});

			l2_gen_ticket = addImpostorGenTicket(ImpostorGenTicket {
				.scaled_pivot = new_pivot_chunk / 4,
				.scaled_distance = Consts::CHUNK_LOAD_DISTANCE_CHUNKS,
				.scale_log2 = 2,
			});

			l3_gen_ticket = addImpostorGenTicket(ImpostorGenTicket {
				.scaled_pivot = new_pivot_chunk / 8,
				.scaled_distance = Consts::CHUNK_LOAD_DISTANCE_CHUNKS,
				.scale_log2 = 3,
			});
		}

		size_t remains = std::min(Consts::MAX_LOADING_ITERATIONS_PER_TICK, chunk_load_tasks.size());

		for (size_t i = 0; i < remains; i++) {
			ChunkKey ck = chunk_load_tasks.front();
			chunk_load_tasks.pop_front();
			processSingleChunkLoading(ck);
		}

		remains = std::min(Consts::MAX_LOADING_ITERATIONS_PER_TICK, impostor_gen_tasks.size());

		for (size_t i = 0; i < remains; i++) {
			ChunkKey ck = impostor_gen_tasks.front();
			impostor_gen_tasks.pop_front();
			processSingleImpostorGen(ck);
		}
	}

	void processChunkUnloading()
	{
		size_t remains = Consts::MAX_UNLOADING_ITERATIONS_PER_TICK;

		for (auto iter = chunk_ticket_dec_walkers.begin(); iter != chunk_ticket_dec_walkers.end() && remains > 0;
			/*nothing*/) {
			auto &walker = *iter;

			while (!walker.second.wrappedAround() && remains > 0) {
				remains--;

				ChunkKey ck(walker.first + walker.second.step());
				auto &metastate = key_metastate[ck];

				if (--metastate.chunk_ticket_refs == 0 && !metastate.has_chunk_load_pending
					&& metastate.chunk_load_dependent_impostor_gens.empty()) {
					state.chunk_table.erase(timeline, ck);
				}

				if (metastate.safeToDelete()) {
					key_metastate.erase(ck);
				}
			}

			if (walker.second.wrappedAround()) {
				iter = chunk_ticket_dec_walkers.erase(iter);
			} else {
				++iter;
			}
		}

		remains = Consts::MAX_UNLOADING_ITERATIONS_PER_TICK;

		for (auto iter = impostor_gen_dec_walkers.begin(); iter != impostor_gen_dec_walkers.end() && remains > 0;
			/*nothing*/) {
			auto &[scaled_pivot, scale_log2, walker] = *iter;

			while (!walker.wrappedAround() && remains > 0) {
				remains--;

				const glm::ivec3 step = walker.step();

				ChunkKey ck((scaled_pivot + step) << int32_t(scale_log2), scale_log2);
				auto &metastate = key_metastate[ck];

				if (--metastate.impostor_ticket_refs == 0 && !metastate.has_impostor_gen_pending
					&& metastate.impostor_gen_dependent_impostor_gens.empty()) {
					state.fake_chunk_data_table.erase(timeline, ck);
				}

				if (metastate.safeToDelete()) {
					key_metastate.erase(ck);
				}
			}

			if (walker.wrappedAround()) {
				iter = impostor_gen_dec_walkers.erase(iter);
			} else {
				++iter;
			}
		}
	}

	ChunkLoadTicketPtr addChunkLoadTicket(ChunkLoadTicket ticket)
	{
		auto radius = uint16_t(std::min((1 << 15) - 1, ticket.distance));

		ConcentricOctahedronWalker walker(radius);
		while (!walker.wrappedAround()) {
			ChunkKey ck(ticket.pivot + walker.step());
			auto &metastate = key_metastate[ck];

			if (metastate.chunk_ticket_refs++ == 0) {
				chunk_load_tasks.emplace_back(ck);
			}
		}

		return ChunkLoadTicketPtr(new ChunkLoadTicket(ticket), [this](ChunkLoadTicket *ticket) {
			auto radius = uint16_t(std::min((1 << 15) - 1, ticket->distance));
			chunk_ticket_dec_walkers.emplace_back(ticket->pivot, ConcentricOctahedronWalker(radius));
			delete ticket;
		});
	}

	ImpostorGenTicketPtr addImpostorGenTicket(ImpostorGenTicket ticket)
	{
		auto radius = uint16_t(std::min((1 << 15) - 1, ticket.scaled_distance));

		ConcentricOctahedronWalker walker(radius);
		while (!walker.wrappedAround()) {
			glm::ivec3 pos = (ticket.scaled_pivot + walker.step()) << int32_t(ticket.scale_log2);
			ChunkKey ck(pos, ticket.scale_log2);

			auto &metastate = key_metastate[ck];
			if (metastate.impostor_ticket_refs++ == 0) {
				impostor_gen_tasks.emplace_back(ck);
			}
		}

		return ImpostorGenTicketPtr(new ImpostorGenTicket(ticket), [this](ImpostorGenTicket *ticket) {
			auto radius = uint16_t(std::min((1 << 15) - 1, ticket->scaled_distance));
			impostor_gen_dec_walkers.emplace_back(ticket->scaled_pivot, ticket->scale_log2,
				ConcentricOctahedronWalker(radius));
			delete ticket;
		});
	}
};

Land::Land() : m_impl(std::make_unique<Impl>())
{
	uint16_t id = m_impl->block_registry.registerBlock(std::make_shared<TestBlock>());
	(void) id;
	// We've just created the registry
	assert(id == 1);
}

Land::~Land() noexcept = default;

void Land::setLoadingPoint(glm::dvec3 point)
{
	m_impl->loading_point = point;
}

void Land::tick()
{
	assert(m_impl);
	auto &impl = *m_impl;

	++impl.timeline;

	impl.processRequestsCompletion();
	impl.processChunkLoading();
	impl.processChunkUnloading();
}

BlockRegistry &Land::blockRegsitry() noexcept
{
	return m_impl->block_registry;
}

const LandState &Land::stateForCopy() const noexcept
{
	return m_impl->state;
}

} // namespace voxen::land
