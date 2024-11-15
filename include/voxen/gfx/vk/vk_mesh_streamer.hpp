#pragma once

#include <voxen/common/uid.hpp>
#include <voxen/gfx/frame_tick_id.hpp>
#include <voxen/gfx/gfx_fwd.hpp>
#include <voxen/gfx/vk/vk_include.hpp>
#include <voxen/gfx/vk/vma_fwd.hpp>
#include <voxen/util/lru_visit_ordering.hpp>

#include <cstdint>
#include <deque>
#include <list>
#include <span>
#include <unordered_map>

namespace voxen::gfx::vk
{

// This class is NOT thread-safe.
//
// TODO: fast(er) path for UMA/ReBAR systems (no transfers, map+write immediately)
class MeshStreamer {
public:
	constexpr static uint32_t MAX_MESH_SUBSTREAMS = 4;
	constexpr static uint32_t MAX_ELEMENT_SIZE = 1024;

	struct MeshSubstreamInfo {
		VkBuffer vk_buffer = VK_NULL_HANDLE;
		uint32_t first_element = 0;
		uint32_t num_elements = 0;
		uint32_t element_size = 0;
	};

	struct MeshInfo {
		int64_t ready_version = -1;
		int64_t pending_version = -1;
		MeshSubstreamInfo substreams[MAX_MESH_SUBSTREAMS];
	};

	struct MeshSubstreamAdd {
		const void *data = nullptr;
		uint32_t num_elements = 0;
		uint32_t element_size = 0;
	};

	struct MeshAdd {
		int64_t version;
		MeshSubstreamAdd substreams[MAX_MESH_SUBSTREAMS];
	};

	MeshStreamer(GfxSystem &gfx);
	MeshStreamer(MeshStreamer &&) = delete;
	MeshStreamer(const MeshStreamer &) = delete;
	MeshStreamer &operator=(MeshStreamer &&) = delete;
	MeshStreamer &operator=(const MeshStreamer &) = delete;
	~MeshStreamer();

	void addMesh(UID key, const MeshAdd &mesh_add);
	bool queryMesh(UID key, MeshInfo &mesh_info);

	void onFrameTickBegin(FrameTickId completed_tick, FrameTickId new_tick);
	void onFrameTickEnd(FrameTickId current_tick);

private:
	struct Pool;

	struct Allocation {
		Pool *pool = nullptr;
		uint32_t range_begin = 0;
		uint32_t range_end = 0;

		bool valid() const noexcept { return pool != nullptr; }
		uint32_t sizeElements() const noexcept { return range_end - range_begin; }
	};

	struct Transfer;

	struct KeyInfo {
		// Frame with the latest possible GPU access to this key
		FrameTickId last_access_tick = FrameTickId::INVALID;
		// Version of data stored in `ready_substream_allocations`
		int64_t ready_version = -1;
		// Allocations of substreams ready for GPU use in this frame tick
		Allocation ready_substream_allocations[MAX_MESH_SUBSTREAMS];
		// Pointer to the latest pending transfer of this key.
		// Its version is greater than (data update) or equal to (defrag move) `ready_version`.
		Transfer *pending_transfer = nullptr;
	};

	struct Pool {
		VkBuffer vk_handle = VK_NULL_HANDLE;
		VmaAllocation vma_handle = VK_NULL_HANDLE;

		// Timestamp of the latest allocation from this pool
		FrameTickId last_allocation_tick = FrameTickId::INVALID;
		// Timestamp of the latest possible GPU access to this pool
		FrameTickId last_access_tick = FrameTickId::INVALID;

		uint32_t allocated_elements = 0;
		uint32_t freed_elements = 0;

		uint32_t element_size : 16 = 0;
		uint32_t is_exhausted : 1 = 0;
		uint32_t needs_defragmentation : 1 = 0;
	};

	struct Transfer {
		// Mesh key being transferred
		UID key;
		// Frame when this transfer was started (ends when this tick completes)
		FrameTickId started_tick;
		// Version of data being written to `substream_allocations`
		int64_t version;
		// Allocations of substreams being written to
		Allocation substream_allocations[MAX_MESH_SUBSTREAMS];
	};

	GfxSystem &m_gfx;
	FrameTickId m_current_tick_id = FrameTickId::INVALID;

	std::unordered_map<UID, KeyInfo> m_key_info_map;
	std::list<Pool> m_pools;
	std::deque<Transfer> m_transfers;
	LruVisitOrdering<UID, FrameTickTag> m_lru_visit_order;

	Allocation allocate(uint32_t num_elements, uint32_t element_size);
	void deallocate(Allocation &alloc) noexcept;
	void deallocate(std::span<Allocation, MAX_MESH_SUBSTREAMS> allocs) noexcept;

	Transfer *transferUpload(UID key, const MeshAdd &mesh_add);
	Transfer *transferDefragment(UID key, KeyInfo &info);
};

} // namespace voxen::gfx::vk
