#include <voxen/gfx/gfx_land_loader.hpp>

#include <voxen/client/vulkan/common.hpp>
#include <voxen/common/land/land.hpp>
#include <voxen/common/land/land_utils.hpp>
#include <voxen/common/world_state.hpp>
#include <voxen/gfx/vk/async_dma.hpp>
#include <voxen/gfx/vk/vk_device.hpp>
#include <voxen/util/log.hpp>

#include <extras/bitset.hpp>
#include <extras/defer.hpp>

#include <vma/vk_mem_alloc.h>

#include <unordered_set>

namespace voxen::gfx
{

// TODO: there parts are not yet moved to voxen/gfx/vk
using client::vulkan::VulkanException;

namespace
{

constexpr size_t FAKE_FACE_BATCHES_IN_POOL = 512;

constexpr size_t FAKE_FACE_SIZE_BYTES = sizeof(land::FakeChunkData::FakeFace);
constexpr size_t FAKE_FACE_BATCH_SIZE_BYTES = FAKE_FACE_SIZE_BYTES * LandLoader::FAKE_FACE_BATCH_SIZE;
constexpr size_t FAKE_FACE_POOL_SIZE_BYTES = FAKE_FACE_BATCH_SIZE_BYTES * FAKE_FACE_BATCHES_IN_POOL;

constexpr double DRAW_DISTANCE_METRES = 2500.0;

// Minimal distance to begin drawing a given impostor level
constexpr double DRAW_DISTANCE_IMPOSTOR[land::Consts::NUM_IMPOSTOR_LEVELS] = {
	250.0,
	500.0,
	1000.0,
	2000.0,
	4000.0,
};

bool needFinerImpostorLevel(double sq_distance, uint8_t level)
{
	if (level > 0) {
		return true;
	}

	double threshold = DRAW_DISTANCE_IMPOSTOR[level];
	return sq_distance < threshold * threshold;
}

struct FakeFaceDataLocations {
	uint32_t num_faces = 0;
	uint32_t num_batches = 0;

	std::vector<std::pair<uint16_t, uint16_t>> batches;
};

struct FakeFacePool {
	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation alloc = VK_NULL_HANDLE;

	extras::bitset<FAKE_FACE_BATCHES_IN_POOL> used_slots;
};

struct FakeFaceUploadInfo {
	uint64_t dma_completion_timeline;
	land::ChunkKey chunk_key;
	uint16_t pool_id;
	uint16_t pool_slot;
};

} // namespace

struct LandLoader::Impl {
	Impl(vk::Device &dev) : dev(dev) {}

	~Impl()
	{
		for (auto &pool : fake_face_pools) {
			dev.enqueueDestroy(pool.buffer, pool.alloc);
		}
	}

	std::pair<uint16_t, uint16_t> allocateFakeFacePoolSlot()
	{
		for (size_t pool = 0; pool < fake_face_pools.size(); pool++) {
			size_t slot = fake_face_pools[pool].used_slots.occupy_zero();
			if (slot != SIZE_MAX) {
				return { static_cast<uint16_t>(pool), static_cast<uint16_t>(slot) };
			}
		}

		// Our pool indices are limited to 16 bits
		assert(fake_face_pools.size() <= UINT16_MAX);

		FakeFacePool &new_pool = fake_face_pools.emplace_back();
		defer_fail {
			dev.enqueueDestroy(new_pool.buffer, new_pool.alloc);
			fake_face_pools.pop_back();
		};

		{
			uint32_t queue_indices[] = {
				dev.info().main_queue_family,
				dev.info().dma_queue_family,
				dev.info().compute_queue_family,
			};

			VkBufferCreateInfo buffer_create_info {
				.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.size = FAKE_FACE_POOL_SIZE_BYTES,
				.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
					| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				.sharingMode = VK_SHARING_MODE_CONCURRENT,
				.queueFamilyIndexCount = std::size(queue_indices),
				.pQueueFamilyIndices = queue_indices,
			};

			VmaAllocationCreateInfo vma_info {};
			vma_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

			VkResult res = vmaCreateBuffer(dev.vma(), &buffer_create_info, &vma_info, &new_pool.buffer, &new_pool.alloc,
				nullptr);
			if (res != VK_SUCCESS) [[unlikely]] {
				throw VulkanException(res, "vmaCreateBuffer");
			}
			defer_fail { vmaDestroyBuffer(dev.vma(), new_pool.buffer, new_pool.alloc); };

			const size_t object_name_index = fake_face_pools.size() + 1;

			char buf[64];
			snprintf(buf, std::size(buf), "land/fake_face_pool_%zu", object_name_index);
			dev.setObjectName(new_pool.buffer, buf);
		}

		size_t slot = new_pool.used_slots.occupy_zero();
		return { static_cast<uint16_t>(fake_face_pools.size() - 1), static_cast<uint16_t>(slot) };
	}

	void writeFakeFaceTransfer(land::ChunkKey key, const land::FakeChunkData &data, vk::AsyncDma &dma)
	{
		auto &loc = fake_face_data_locations[key];

		loc.num_faces = uint32_t(data.faces().size());
		loc.num_batches = 0;

		auto faces_span = data.faces().as_bytes();

		while (!faces_span.empty()) {
			loc.num_batches++;

			auto [pool, slot] = allocateFakeFacePoolSlot();

			size_t cutoff = std::min(FAKE_FACE_BATCH_SIZE_BYTES, faces_span.size());

			dma.transfer({
				.src_buffer = faces_span.subspan(0, cutoff),
				.dst_buffer = fake_face_pools[pool].buffer,
				.dst_offset = FAKE_FACE_BATCH_SIZE_BYTES * slot,
			});

			FakeFaceUploadInfo upload_info;
			upload_info.chunk_key = key;
			upload_info.pool_id = pool;
			upload_info.pool_slot = slot;

			fake_face_upload_infos.emplace_back(upload_info);
			faces_span = faces_span.subspan(cutoff);
		}
	}

	void onNewState(std::shared_ptr<const WorldState> state, vk::AsyncDma &dma)
	{
		size_t first_upload_request = fake_face_upload_infos.size();

		auto erase_key = [&](land::ChunkKey key) {
			empty_chunk_keys.erase(key);

			{
				land::ChunkKey ck = key;
				while (ck.scale_log2 < land::Consts::NUM_IMPOSTOR_LEVELS) {
					auto iter = known_subtrees.find(ck);
					assert(iter != known_subtrees.end());
					if (--(iter->second) == 0) {
						known_subtrees.erase(iter);
					}

					ck = ck.parentLodKey();
				}
			}

			auto iter = fake_face_data_locations.find(key);
			if (iter != fake_face_data_locations.end()) {
				for (auto [pool, slot] : iter->second.batches) {
					fake_face_pools[pool].used_slots.clear(slot);
				}

				fake_face_data_locations.erase(iter);
			}

			// TODO: did we erase everything?
		};

		const auto &new_land = state->landState();

		using FakeDataItem = land::LandState::FakeChunkDataTable::Item;

		new_land.fake_chunk_data_table.visitDiff(last_known_fake_data_table,
			[&](const FakeDataItem *new_item, const FakeDataItem *old_item) {
				if (!new_item) {
					erase_key(old_item->key());
					return true;
				}

				{
					land::ChunkKey ck = new_item->key();
					while (ck.scale_log2 < land::Consts::NUM_IMPOSTOR_LEVELS) {
						known_subtrees[ck]++;
						ck = ck.parentLodKey();
					}
				}

				if (new_item->hasValue()) {
					writeFakeFaceTransfer(new_item->key(), new_item->value(), dma);
				} else {
					// No value - this subspace is empty
					empty_chunk_keys.insert(new_item->key());
				}

				return true;
			});

		size_t last_upload_request = fake_face_upload_infos.size();

		if (last_upload_request > first_upload_request) {
			uint64_t timeline = dma.flush();

			while (first_upload_request < last_upload_request) {
				fake_face_upload_infos[first_upload_request].dma_completion_timeline = timeline;
				first_upload_request++;
			}
		}

		last_known_fake_data_table = new_land.fake_chunk_data_table;
		last_known_chunk_table = new_land.chunk_table;
	}

	void onNewFrame()
	{
		uint64_t dma_timeline = dev.getCompletedTimeline(vk::Device::QueueDma);

		auto iter = fake_face_upload_infos.begin();
		auto end = fake_face_upload_infos.end();

		while (iter != end) {
			if (iter->dma_completion_timeline > dma_timeline) {
				break;
			}

			FakeFaceDataLocations &loc = fake_face_data_locations[iter->chunk_key];
			loc.batches.emplace_back(iter->pool_id, iter->pool_slot);

			++iter;
		}

		fake_face_upload_infos.erase(fake_face_upload_infos.begin(), iter);
	}

	bool makeRenderListGeometry(RenderList &rlist, glm::ivec3 chunk_base)
	{
		(void) rlist;
		(void) chunk_base;
		// True geometry, add it or we can't draw the chunk at all
		// TODO: geometry is not yet implemented
		return false;
	}

	bool requestImpostorFaces(const glm::dvec3 &view_dir, land::ChunkKey key, std::vector<RenderCmd> &cmd)
	{
		const FakeFaceDataLocations &loc = fake_face_data_locations[key];

		if (loc.batches.size() < loc.num_batches) {
			// TODO: issue loading requests
			return false;
		}

		RenderCmd rcmd;

		const glm::ivec3 base = key.base();
		rcmd.chunk_base_x = base.x;
		rcmd.chunk_base_y = base.y;
		rcmd.chunk_base_z = base.z;

		const uint32_t x_face_bit = view_dir.x >= 0.0 ? 1 : 0; // X+/X-
		const uint32_t y_face_bit = view_dir.y >= 0.0 ? 2 : 0; // Y+/Y-
		const uint32_t z_face_bit = view_dir.z >= 0.0 ? 4 : 0; // Z+/Z-

		rcmd.chunk_lod = key.scale_log2;
		rcmd.face_mask = x_face_bit | y_face_bit | z_face_bit;

		uint32_t faces_remaining = loc.num_faces;

		for (const auto &batch : loc.batches) {
			rcmd.num_faces = std::min<uint32_t>(faces_remaining, FAKE_FACE_BATCH_SIZE);
			rcmd.data_pool_id = batch.first;
			rcmd.data_pool_slot = batch.second;
			cmd.emplace_back(rcmd);

			faces_remaining -= FAKE_FACE_BATCH_SIZE;
		}

		return true;
	}

	bool makeRenderListImpostor(const glm::dvec3 &viewpoint, RenderList &rlist, land::ChunkKey key)
	{
		const glm::ivec3 chunk_base = key.base();
		uint8_t level = uint8_t(key.scaleLog2());

		auto try_finer_level = [&]() {
			if (level == 0) {
				return makeRenderListGeometry(rlist, chunk_base);
			}

			const uint8_t n = level - 1;
			const int32_t k = 1 << n;

			bool res = makeRenderListImpostor(viewpoint, rlist, land::ChunkKey(chunk_base, n));
			res &= makeRenderListImpostor(viewpoint, rlist, land::ChunkKey(chunk_base + glm::ivec3(k, 0, 0), n));
			res &= makeRenderListImpostor(viewpoint, rlist, land::ChunkKey(chunk_base + glm::ivec3(0, k, 0), n));
			res &= makeRenderListImpostor(viewpoint, rlist, land::ChunkKey(chunk_base + glm::ivec3(0, 0, k), n));
			res &= makeRenderListImpostor(viewpoint, rlist, land::ChunkKey(chunk_base + glm::ivec3(k, k, 0), n));
			res &= makeRenderListImpostor(viewpoint, rlist, land::ChunkKey(chunk_base + glm::ivec3(0, k, k), n));
			res &= makeRenderListImpostor(viewpoint, rlist, land::ChunkKey(chunk_base + glm::ivec3(k, 0, k), n));
			res &= makeRenderListImpostor(viewpoint, rlist, land::ChunkKey(chunk_base + glm::ivec3(k, k, k), n));

			return res;
		};

		if (empty_chunk_keys.contains(key)) {
			// This part of space is empty (from rendering side, i.e. has no visible surface)
			return true;
		}

		if (!known_subtrees.contains(key)) {
			return false;
		}

		const double lod_size = land::Consts::CHUNK_SIZE_METRES * double(1 << level);
		const glm::dvec3 lod_center = glm::dvec3(chunk_base) * land::Consts::CHUNK_SIZE_METRES + 0.5 * lod_size;

		const glm::dvec3 view_dir = viewpoint - lod_center;
		const double sq_distance = glm::dot(view_dir, view_dir);

		if (sq_distance <= lod_size * lod_size) {
			// Impostors can be somewhat correct only when viewed from outside
			//return try_finer_level();
		}

		std::vector<RenderCmd> cmd;

		bool need_finer_level = needFinerImpostorLevel(sq_distance, level);
		bool have_current_data = requestImpostorFaces(view_dir, key, cmd);

		if (!need_finer_level) {
			// This is the target level

			if (!have_current_data) {
				// TODO: try substituting with finer levels. We need "weak" face data requests
				// (update usage time upon cache hit; don't request otherwise) for that.
				//
				// We don't want to issue tons of fine levels' requests during transients
				// like the initial loading (before the target coarse levels are ready).
				return false;
			}

			for (const RenderCmd &rcmd : cmd) {
				rlist.emplace_back(rcmd);
			}
			return true;
		}

		// This level is coarser than the target

		if (!have_current_data) {
			// Simply recurse, we can't fully substitute a missing finer level.
			// If a certain chunk is missing all the way down to the geometry, there will be
			// a non-rendered hole in its place (there is not enough information to draw it).
			//
			// Should occur only during the initial loading and in some brief transients,
			// because we request data for coarser levels as we traverse the tree.
			return try_finer_level();
		}

		// Some of the finer ones might be missing but we can substitute it with the current level.
		// Remember rlist position and prepare to unwind it upon failure.
		auto rewind_position = ptrdiff_t(rlist.size());

		if (try_finer_level() == true) {
			// Enough data at finer levels
			return true;
		}

		// Finer levels will have Z-fighting or other issues from overlap
		// with the coarser one, so remove potentially added commands.
		// Note that their data requests are still valid and should not be cancelled.
		rlist.erase(rlist.begin() + rewind_position, rlist.end());

		for (const RenderCmd &rcmd : cmd) {
			rlist.emplace_back(rcmd);
		}
		return true;
	}

	void makeRenderList(const glm::dvec3 &viewpoint, RenderList &rlist)
	{
		{
			static int kek = 0;
			if (kek++ == 200) {
				kek = 0;

				const glm::ivec3 pchunk = glm::ivec3(glm::floor(viewpoint / land::Consts::CHUNK_SIZE_METRES));
				const land::ChunkKey ck(pchunk, 0);

				char buf_idl[128];

				auto iter = fake_face_data_locations.find(ck);
				if (iter != fake_face_data_locations.end()) {
					snprintf(buf_idl, std::size(buf_idl), "F%u/B%u/A%zu", iter->second.num_faces, iter->second.num_batches,
						iter->second.batches.size());
				} else {
					snprintf(buf_idl, std::size(buf_idl), "N/A");
				}

				char eck = empty_chunk_keys.contains(ck) ? '+' : '-';

				const auto *item = last_known_fake_data_table.find(ck);
				char imp = item ? '+' : '-';
				char impp = item ? (item->hasValue() ? '+' : '-') : 'X';

				const auto *item2 = last_known_chunk_table.find(ck);
				char ctp = item2 ? '+' : '-';
				char ctpp = item2 ? (item2->hasValue() ? '+' : '-') : 'X';

				Log::info("CK {} {} {} | IDL {} | ECK {} | IMP {}/{} | CT {}/{}", ck.x, ck.y, ck.z, buf_idl, eck, imp, impp,
					ctp, ctpp);
			}
		}

		rlist.clear();

		constexpr double RSIZE = land::Consts::REGION_SIZE_METRES;
		constexpr double DRAW_DISTANCE_REGIONS = DRAW_DISTANCE_METRES / RSIZE;

		const int32_t region_x_min = int32_t(viewpoint.x / RSIZE - 0.5 - DRAW_DISTANCE_REGIONS);
		const int32_t region_x_max = int32_t(viewpoint.x / RSIZE - 0.5 + DRAW_DISTANCE_REGIONS);

		for (int32_t rx = region_x_min; rx <= region_x_max; rx++) {
			const double y_band_squared = DRAW_DISTANCE_REGIONS * DRAW_DISTANCE_REGIONS - rx * rx;
			const double y_band = std::sqrt(y_band_squared);

			const int32_t region_y_min = int32_t(viewpoint.y / RSIZE - 0.5 - y_band);
			const int32_t region_y_max = int32_t(viewpoint.y / RSIZE - 0.5 + y_band);

			for (int32_t ry = region_y_min; ry <= region_y_max; ry++) {
				const double z_band = std::sqrt(y_band_squared - ry * ry);

				const int32_t region_z_min = int32_t(viewpoint.z / RSIZE - 0.5 - z_band);
				const int32_t region_z_max = int32_t(viewpoint.z / RSIZE - 0.5 + z_band);

				for (int32_t rz = region_z_min; rz <= region_z_max; rz++) {
					glm::ivec3 chunk_base = glm::ivec3(rx, ry, rz) * land::Consts::REGION_SIZE_CHUNKS;
					land::ChunkKey key(chunk_base, land::Consts::NUM_IMPOSTOR_LEVELS - 1);
					makeRenderListImpostor(viewpoint, rlist, key);
				}
			}
		}
	}

	vk::Device &dev;

	land::LandState::ChunkTable last_known_chunk_table;
	land::LandState::FakeChunkDataTable last_known_fake_data_table;

	std::unordered_map<land::ChunkKey, FakeFaceDataLocations> fake_face_data_locations;
	std::unordered_set<land::ChunkKey> empty_chunk_keys;
	std::unordered_map<land::ChunkKey, size_t> known_subtrees;
	std::vector<FakeFacePool> fake_face_pools;
	std::vector<FakeFaceUploadInfo> fake_face_upload_infos;
};

LandLoader::LandLoader(vk::Device &dev) : m_impl(dev) {}

LandLoader::~LandLoader() noexcept = default;

void LandLoader::onNewState(std::shared_ptr<const WorldState> state, vk::AsyncDma &dma)
{
	m_impl->onNewState(std::move(state), dma);
}

void LandLoader::onNewFrame()
{
	m_impl->onNewFrame();
}

void LandLoader::makeRenderList(const glm::dvec3 &viewpoint, RenderList &rlist)
{
	m_impl->makeRenderList(viewpoint, rlist);
}

VkBuffer LandLoader::getFakeDataPool(uint32_t pool)
{
	return m_impl->fake_face_pools[pool].buffer;
}

} // namespace voxen::gfx
