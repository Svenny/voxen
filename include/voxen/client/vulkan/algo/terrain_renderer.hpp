#pragma once

#include <voxen/client/vulkan/buffer.hpp>
#include <voxen/client/vulkan/config.hpp>
#include <voxen/client/vulkan/memory.hpp>
#include <voxen/common/terrain/chunk_id.hpp>

#include <extras/refcnt_ptr.hpp>

#include <glm/vec4.hpp>

#include <memory>
#include <unordered_map>
#include <vector>

namespace voxen
{

class Aabb;
class GameView;
class WorldState;

namespace terrain
{

class Chunk;

}

}

namespace voxen::client::vulkan
{

class TerrainRenderer final {
public:
	TerrainRenderer();
	TerrainRenderer(TerrainRenderer &&) = delete;
	TerrainRenderer(const TerrainRenderer &) = delete;
	TerrainRenderer &operator = (TerrainRenderer &&) = delete;
	TerrainRenderer &operator = (const TerrainRenderer &) = delete;
	~TerrainRenderer() = default;

	void onNewWorldState(const WorldState &state);
	void onFrameBegin(const GameView &view);

	void prepareResources(VkCommandBuffer cmdbuf);
	void launchFrustumCull(VkCommandBuffer cmdbuf);
	void drawChunksInFrustum(VkCommandBuffer cmdbuf);
	void drawDebugChunkBorders(VkCommandBuffer cmdbuf);

private:
	struct ChunkSlotSyncData {
		uint32_t version;
		uint32_t seam_version;

		uint16_t vertex_arena_id;
		uint16_t index_arena_id;

		VkIndexType index_type;
		uint32_t vertex_arena_offset;
		uint32_t index_arena_offset;

		uint32_t num_vertices;
		uint32_t num_indices;
	};

	struct ChunkSyncData {
		extras::refcnt_ptr<terrain::Chunk> upload_ptr;
		uint32_t idle_age;
		ChunkSlotSyncData slot_a;
		ChunkSlotSyncData slot_b;
	};

	const WorldState *m_last_state = nullptr;
	std::unordered_map<terrain::ChunkId, ChunkSyncData> m_chunk_sync_data;
	std::unordered_map<terrain::ChunkId, uint32_t> m_kek_tmp;

	FatVkBuffer m_debug_octree_mesh_buffer;
	// Stores N copies of:
	// - Chunk transform (base+scale) instance buffer
	// - Indirect draw commands buffer
	// - Chunk AABB buffer
	WrappedVkBuffer m_combo_buffer;
	DeviceAllocation m_combo_buffer_memory;
	// Pointers into `m_combo_buffer` corresponding to different buffer subsections
	void *m_combo_buffer_host_ptr;
	glm::vec4 *m_chunk_transform_ptr[Config::NUM_CPU_PENDING_FRAMES];
	VkDrawIndexedIndirectCommand *m_draw_command_ptr[Config::NUM_CPU_PENDING_FRAMES];
	Aabb *m_chunk_aabb_ptr[Config::NUM_CPU_PENDING_FRAMES];

	uint32_t m_num_active_chunks = 0;
};

}
