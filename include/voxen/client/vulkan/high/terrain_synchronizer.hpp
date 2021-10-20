#pragma once

#include <voxen/client/vulkan/common.hpp>
#include <voxen/client/vulkan/buffer.hpp>
#include <voxen/common/terrain/chunk.hpp>

#include <extras/refcnt_ptr.hpp>

#include <functional>
#include <list>
#include <unordered_map>

namespace voxen::client::vulkan
{

// NOTE: this class is internal to `TerrainSynchronizer` implementation and is not used in any public API
class TerrainDataArena;

class TerrainSynchronizer {
public:
	struct ChunkRenderInfo {
		VkBuffer vertex_buffer;
		VkBuffer index_buffer;
		VkIndexType index_type;
		// Using `int32_t` to match `VkDrawIndexedIndirectCommand`
		int32_t first_vertex;
		uint32_t first_index;
		uint32_t num_vertices;
		uint32_t num_indices;
	};

	TerrainSynchronizer();
	TerrainSynchronizer(TerrainSynchronizer &&) = delete;
	TerrainSynchronizer(const TerrainSynchronizer &) = delete;
	TerrainSynchronizer &operator = (TerrainSynchronizer &&) = delete;
	TerrainSynchronizer &operator = (const TerrainSynchronizer &) = delete;
	~TerrainSynchronizer() noexcept;

	void beginSyncSession();
	// It's not allowed to sync empty chunks (which have `hasSurfaceStrict() == false`)
	ChunkRenderInfo syncChunk(const extras::refcnt_ptr<terrain::Chunk> &chunk);
	void endSyncSession();

	// This method is called by `TerrainDataArena` when it is completely free.
	// Arena is deleted after calling this, so in some sense it's similar to `delete this`.
	// NOTE: this is an internal API exposed in public section, do not call it.
	void arenaFreeCallback(TerrainDataArena *arena) noexcept;

private:
	struct ChunkSlotSyncData {
		uint16_t vertex_arena_id = UINT16_MAX;
		uint16_t index_arena_id = UINT16_MAX;

		VkIndexType index_type = VK_INDEX_TYPE_NONE_KHR;
		uint32_t vertex_arena_offset = 0;
		uint32_t vertex_arena_range = 0;
		uint32_t index_arena_offset = 0;
		uint32_t index_arena_range = 0;

		uint32_t num_vertices = 0;
		uint32_t num_indices = 0;
	};

	struct PerChunkData {
		ChunkSlotSyncData slot_active;
		ChunkSlotSyncData slot_loading;
		extras::refcnt_ptr<terrain::Chunk> last_chunk;
		uint32_t loading_request_id = UINT32_MAX;
		uint32_t slot_switch_age = UINT32_MAX;
	};

	uint32_t m_sync_age = 0;

	uint32_t m_queue_families[2];
	bool m_vertex_uma = false;
	bool m_index_uma = false;
	std::list<TerrainDataArena> m_vertex_arenas;
	std::list<TerrainDataArena> m_index_arenas;
	std::unordered_map<terrain::ChunkId, PerChunkData> m_per_chunk_data;

	void clearSlot(ChunkSlotSyncData &slot) noexcept;
	void allocateSlot(ChunkSlotSyncData &slot, VkDeviceSize vtx_size, VkDeviceSize idx_size);
	void makeSurfaceTransfer(ChunkSlotSyncData &slot, const terrain::Chunk &chunk);
	ChunkRenderInfo slotToRenderInfo(const ChunkSlotSyncData &slot) const noexcept;

	void addVertexArena();
	void addIndexArena();

	static TerrainDataArena &selectArena(std::list<TerrainDataArena> &list, uint32_t id) noexcept;
	static VkBuffer arenaHandle(const std::list<TerrainDataArena> &list, uint32_t id) noexcept;
};

}
