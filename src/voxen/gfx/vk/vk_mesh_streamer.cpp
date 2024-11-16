#include <voxen/gfx/vk/vk_mesh_streamer.hpp>

#include <voxen/gfx/gfx_system.hpp>
#include <voxen/gfx/vk/vk_device.hpp>
#include <voxen/gfx/vk/vk_dma_system.hpp>
#include <voxen/gfx/vk/vk_error.hpp>
#include <voxen/gfx/vk/vk_utils.hpp>

#include <extras/defer.hpp>

#include <vma/vk_mem_alloc.h>

#include <cassert>
#include <utility>

namespace voxen::gfx::vk
{

namespace
{

constexpr uint32_t POOL_SIZE_BYTES = 16 * 1024 * 1024;

// Keys not accessed for more than this number of ticks
// are considered stale and will get evicted from the pool
constexpr int64_t STALE_KEY_AGE_THRESHOLD = 200;
// Pools not allocated from for more than this number of ticks
// are considered stale and will get removed to scale down memory usage
constexpr int64_t STALE_POOL_AGE_THRESHOLD = 500;
// Visit a small batch of keys every tick to spread cleanup workload over time
constexpr size_t KEY_CLEANUP_STEPS_PER_TICK = 32;
// Kick off pool defragmentation when free/total ratio exceeds this threshold
constexpr float POOL_DEFRAGMENTATION_FREE_RATIO_THRESHOLD = 0.25f;

} // namespace

MeshStreamer::MeshStreamer(GfxSystem &gfx) : m_gfx(gfx) {}

MeshStreamer::~MeshStreamer()
{
	for (Pool &pool : m_pools) {
		m_gfx.device()->enqueueDestroy(pool.vk_handle, pool.vma_handle);
	}
}

void MeshStreamer::addMesh(UID key, const MeshAdd &mesh_add)
{
	assert(mesh_add.version >= 0);

	KeyInfo &info = m_key_info_map[key];
	if (info.last_access_tick.invalid()) {
		// Never accessed before - register in cleanup/defrag visit ordering,
		// visiting not earlier than it can theoretically become stale
		m_lru_visit_order.addKey(key, m_current_tick_id + STALE_KEY_AGE_THRESHOLD);
	}

	info.last_access_tick = m_current_tick_id;

	// Version must be strictly increasing
	assert(info.ready_version < mesh_add.version);
	if (info.pending_transfer) {
		assert(info.pending_transfer->version < mesh_add.version);
	}

	// If there was a pending transfer it will complete first, then this one.
	// Pointer only stores the latest pending transfer to eliminate unnecessary defrags.
	info.pending_transfer = transferUpload(key, mesh_add);
}

bool MeshStreamer::queryMesh(UID key, MeshInfo &mesh_info)
{
	// Clear all fields
	mesh_info = MeshInfo {};

	auto iter = m_key_info_map.find(key);
	if (iter == m_key_info_map.end()) {
		return false;
	}

	KeyInfo &info = iter->second;
	info.last_access_tick = m_current_tick_id;

	if (info.ready_version >= 0) {
		mesh_info.ready_version = info.ready_version;

		for (uint32_t i = 0; i < MAX_MESH_SUBSTREAMS; i++) {
			MeshSubstreamInfo &substream = mesh_info.substreams[i];
			if (info.ready_substream_allocations[i].valid()) {
				info.ready_substream_allocations[i].pool->last_access_tick = m_current_tick_id;

				substream.vk_buffer = info.ready_substream_allocations[i].pool->vk_handle;
				substream.first_element = info.ready_substream_allocations[i].range_begin;
				substream.num_elements = info.ready_substream_allocations[i].sizeElements();
				substream.element_size = info.ready_substream_allocations[i].pool->element_size;
			}
		}
	}

	if (info.pending_transfer) {
		mesh_info.pending_version = info.pending_transfer->version;
	}

	return true;
}

void MeshStreamer::onFrameTickBegin(FrameTickId completed_tick, FrameTickId new_tick)
{
	// Update tick ID before doing operations below, they can allocate or enqueue transfers
	m_current_tick_id = new_tick;

	// Process transfer completions
	while (!m_transfers.empty()) {
		Transfer &tx = m_transfers.front();
		if (tx.started_tick > completed_tick) {
			// Transfers are ordered by timestamps, all following ones are not complete yet
			break;
		}

		auto iter = m_key_info_map.find(tx.key);
		// Key could go away during the transfer
		if (iter != m_key_info_map.end()) {
			KeyInfo &info = iter->second;

			assert(tx.version >= info.ready_version);
			info.ready_version = tx.version;

			for (uint32_t i = 0; i < MAX_MESH_SUBSTREAMS; i++) {
				deallocate(info.ready_substream_allocations[i]);
				info.ready_substream_allocations[i] = tx.substream_allocations[i];
			}

			// Don't unset this pointer if another transfer was enqueued after this one.
			// This pointer serves just as a flag to eliminate unneeded defrag transfers.
			if (info.pending_transfer == &tx) {
				info.pending_transfer = nullptr;
			}
		} else {
			// We'll, we're a bit late
			deallocate(tx.substream_allocations);
		}

		m_transfers.pop_front();
	}

	// Process stale keys and defragmentations.
	m_lru_visit_order.visitOldest(
		[&](UID key) -> FrameTickId {
			auto iter = m_key_info_map.find(key);
			if (iter == m_key_info_map.end()) {
				// Key has gone away (but how?)
				return FrameTickId::INVALID;
			}

			KeyInfo &info = iter->second;
			if (info.last_access_tick + STALE_KEY_AGE_THRESHOLD <= completed_tick) {
				// Stale key, drop it
				deallocate(info.ready_substream_allocations);
				m_key_info_map.erase(iter);
				// Tell `m_lru_visit_order` to remove it from visit schedule
				return FrameTickId::INVALID;
			}

			// Too young to die. But maybe its pool needs defragmentation?
			// Then request to transfer its allocations somewhere else.
			// Don't do this if there is another pending transfer
			// as this will get deallocated soon anyway.
			if (info.ready_version >= 0 && !info.pending_transfer) {
				// TODO: implement separate substream transfers.
				// Currently transfers are all-or-nothing so we have to move all
				// even if just one of the pools needs defragmentation.
				for (uint32_t i = 0; i < MAX_MESH_SUBSTREAMS; i++) {
					if (info.ready_substream_allocations[i].valid()
						&& info.ready_substream_allocations[i].pool->needs_defragmentation) {
						// Note - this function updates access tick
						info.pending_transfer = transferDefragment(iter->first, info);
						break;
					}
				}
			}

			// Don't visit it again earlier than it can become stale
			return info.last_access_tick + STALE_KEY_AGE_THRESHOLD;
		},
		// Visit a few keys per tick, cut off at GPU complete tick ID
		KEY_CLEANUP_STEPS_PER_TICK, completed_tick);

	// Process stale and emptied pools, and flag them for defragmentation.
	// Iterate over every pool - we don't expect to have many.
	for (auto iter = m_pools.begin(); iter != m_pools.end(); /*nothing*/) {
		Pool &pool = *iter;

		if (pool.allocated_elements > 0 && pool.allocated_elements == pool.freed_elements
			&& pool.last_access_tick <= completed_tick) {
			// Everything freed and no longer accessed, reset the pool
			pool.allocated_elements = 0;
			pool.freed_elements = 0;
			pool.is_exhausted = 0;
			pool.needs_defragmentation = 0;
			// Allow it to be repurposed for a different element size
			pool.element_size = 0;
		}

		// Don't start defragmentation until at least one allocation could not be served
		if (pool.is_exhausted) {
			uint32_t total_space = POOL_SIZE_BYTES / pool.element_size;
			uint32_t free_space = total_space - pool.allocated_elements + pool.freed_elements;
			float free_ratio = static_cast<float>(free_space) / static_cast<float>(total_space);

			if (free_ratio > POOL_DEFRAGMENTATION_FREE_RATIO_THRESHOLD) {
				// This pool wastes too much free space, let's defragment it
				pool.needs_defragmentation = 1;
			}
		}

		if (pool.allocated_elements == 0 && pool.last_allocation_tick + STALE_POOL_AGE_THRESHOLD <= completed_tick) {
			// Stale pool (nothing is allocated for a long time), destroy it directly, no need to enqueue
			vmaDestroyBuffer(m_gfx.device()->vma(), pool.vk_handle, pool.vma_handle);
			iter = m_pools.erase(iter);
		} else {
			++iter;
		}
	}
}

void MeshStreamer::onFrameTickEnd(FrameTickId /*current_tick*/)
{
	// Nothing
}

auto MeshStreamer::allocate(uint32_t num_elements, uint32_t element_size) -> Allocation
{
	assert(element_size > 0);
	assert(element_size <= MAX_ELEMENT_SIZE);
	assert(num_elements * element_size <= POOL_SIZE_BYTES);

	// Iterate over every pool - we don't expect to have many
	for (auto iter = m_pools.begin(); iter != m_pools.end(); ++iter) {
		Pool &pool = *iter;

		if (pool.needs_defragmentation) {
			// Don't allocate from pools that are defragmenting now
			continue;
		}

		if (pool.element_size == 0) {
			// Empty pool, repurpose it for our element size
			pool.element_size = element_size;
		} else if (pool.element_size != element_size) {
			// Not our element size
			continue;
		}

		if ((pool.allocated_elements + num_elements) * element_size <= POOL_SIZE_BYTES) {
			Allocation alloc {
				.pool = &pool,
				.range_begin = pool.allocated_elements,
				.range_end = pool.allocated_elements + num_elements,
			};

			pool.last_allocation_tick = m_current_tick_id;
			pool.last_access_tick = m_current_tick_id;
			pool.allocated_elements += num_elements;

			return alloc;
		} else {
			// At least one allocation from this pool failed,
			// mark it so it can get defragmented later
			pool.is_exhausted = 1;
		}
	}

	// Out of pool space, create a new one
	auto &pool = m_pools.emplace_back();
	defer_fail { m_pools.pop_back(); };

	auto &dev = *m_gfx.device();

	VkBufferCreateInfo buffer_create_info {};
	buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_create_info.size = POOL_SIZE_BYTES;
	buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
		| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	VulkanUtils::fillBufferSharingInfo(dev, buffer_create_info);

	VmaAllocationCreateInfo alloc_create_info {};
	alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	VkResult res = vmaCreateBuffer(dev.vma(), &buffer_create_info, &alloc_create_info, &pool.vk_handle, &pool.vma_handle,
		nullptr);
	if (res != VK_SUCCESS) [[unlikely]] {
		throw VulkanException(res, "vmaCreateBuffer");
	}

	defer_fail { vmaDestroyBuffer(dev.vma(), pool.vk_handle, pool.vma_handle); };

	auto disambig = VulkanUtils::makeHandleDisambiguationString(pool.vk_handle);
	char name_buf[64];
	snprintf(name_buf, std::size(name_buf), "streaming/mesh/pool@%s", disambig.data());
	dev.setObjectName(pool.vk_handle, name_buf);

	pool.last_allocation_tick = m_current_tick_id;
	pool.last_access_tick = m_current_tick_id;
	pool.allocated_elements = num_elements;
	pool.element_size = element_size;

	return Allocation {
		.pool = &pool,
		.range_begin = 0,
		.range_end = num_elements,
	};
}

void MeshStreamer::deallocate(Allocation &alloc) noexcept
{
	if (alloc.valid()) {
		alloc.pool->freed_elements += alloc.sizeElements();
		alloc = Allocation {};
	}
}

void MeshStreamer::deallocate(std::span<Allocation, MAX_MESH_SUBSTREAMS> allocs) noexcept
{
	for (uint32_t i = 0; i < MAX_MESH_SUBSTREAMS; i++) {
		if (allocs[i].valid()) {
			allocs[i].pool->freed_elements += allocs[i].sizeElements();
			allocs[i] = Allocation {};
		}
	}
}

auto MeshStreamer::transferUpload(UID key, const MeshAdd &mesh_add) -> Transfer *
{
	Transfer &tx = m_transfers.emplace_back();
	tx.key = key;
	tx.started_tick = m_current_tick_id;
	tx.version = mesh_add.version;

	defer_fail{
		deallocate(tx.substream_allocations);
		m_transfers.pop_back();
	};

	for (uint32_t i = 0; i < MAX_MESH_SUBSTREAMS; i++) {
		auto &substream = mesh_add.substreams[i];

		if (substream.num_elements == 0) {
			continue;
		}

		const uint32_t element_size = substream.element_size;
		tx.substream_allocations[i] = allocate(substream.num_elements, element_size);

		m_gfx.dmaSystem()->uploadToBuffer({
			.src_data = substream.data,
			.dst_buffer = tx.substream_allocations[i].pool->vk_handle,
			.dst_offset = tx.substream_allocations[i].range_begin * element_size,
			.size = substream.num_elements * element_size,
		});
	}

	return &tx;
}

auto MeshStreamer::transferDefragment(UID key, KeyInfo &info) -> Transfer *
{
	Transfer &tx = m_transfers.emplace_back();
	tx.key = key;
	tx.started_tick = m_current_tick_id;
	tx.version = info.ready_version;

	defer_fail{
		deallocate(tx.substream_allocations);
		m_transfers.pop_back();
	};

	for (uint32_t i = 0; i < MAX_MESH_SUBSTREAMS; i++) {
		auto &substream_alloc = info.ready_substream_allocations[i];

		if (!substream_alloc.valid()) {
			continue;
		}

		// Mark source pool as GPU accessed so that it does not get freed in the middle of transfer
		substream_alloc.pool->last_access_tick = m_current_tick_id;

		const uint32_t element_size = substream_alloc.pool->element_size;
		tx.substream_allocations[i] = allocate(substream_alloc.sizeElements(), element_size);

		m_gfx.dmaSystem()->copyBufferToBuffer({
			.src_buffer = info.ready_substream_allocations[i].pool->vk_handle,
			.dst_buffer = tx.substream_allocations[i].pool->vk_handle,
			.src_offset = info.ready_substream_allocations[i].range_begin * element_size,
			.dst_offset = tx.substream_allocations[i].range_begin * element_size,
			.size = tx.substream_allocations[i].sizeElements() * element_size,
		});
	}

	return &tx;
}

} // namespace voxen::gfx::vk
