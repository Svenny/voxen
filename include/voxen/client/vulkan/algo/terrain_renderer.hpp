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
	using ChunkPtr = extras::refcnt_ptr<terrain::Chunk>;

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
	const WorldState *m_last_state = nullptr;

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

	std::vector<std::tuple<uint32_t, VkBuffer, VkBuffer, VkIndexType>> m_draw_setups;
	uint32_t m_num_active_chunks = 0;
};

}
