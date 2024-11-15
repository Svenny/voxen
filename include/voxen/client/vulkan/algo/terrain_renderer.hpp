#pragma once

#include <voxen/gfx/vk/vk_transient_buffer_allocator.hpp>
#include <voxen/gfx/vk/vma_fwd.hpp>

#include <extras/refcnt_ptr.hpp>

#include <glm/vec4.hpp>

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

} // namespace voxen

namespace voxen::client::vulkan
{

class TerrainRenderer final {
public:
	using ChunkPtr = extras::refcnt_ptr<terrain::Chunk>;

	TerrainRenderer();
	TerrainRenderer(TerrainRenderer &&) = delete;
	TerrainRenderer(const TerrainRenderer &) = delete;
	TerrainRenderer &operator=(TerrainRenderer &&) = delete;
	TerrainRenderer &operator=(const TerrainRenderer &) = delete;
	~TerrainRenderer();

	void onNewWorldState(const WorldState &state);
	void onFrameBegin(const GameView &view, VkDescriptorSet main_scene_dset, VkDescriptorSet frustum_cull_dset);

	void prepareResources(VkCommandBuffer cmdbuf);
	void launchFrustumCull(VkCommandBuffer cmdbuf);
	void drawChunksInFrustum(VkCommandBuffer cmdbuf);
	void drawDebugChunkBorders(VkCommandBuffer cmdbuf);

private:
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

	const WorldState *m_last_state = nullptr;

	VkBuffer m_debug_octree_mesh_buffer = VK_NULL_HANDLE;
	VmaAllocation m_debug_octree_mesh_alloc = VK_NULL_HANDLE;

	VkDeviceSize m_buffer_alloc_alignment = 0;
	gfx::vk::TransientBufferAllocator::Allocation m_chunk_transform_buffer;
	gfx::vk::TransientBufferAllocator::Allocation m_draw_command_buffer;
	gfx::vk::TransientBufferAllocator::Allocation m_chunk_aabb_buffer;

	std::vector<std::pair<const terrain::Chunk *, ChunkRenderInfo>> m_render_infos;
	std::vector<std::tuple<uint32_t, VkBuffer, VkBuffer, VkIndexType>> m_draw_setups;

	VkDescriptorSet m_main_scene_dset = VK_NULL_HANDLE;
	VkDescriptorSet m_frustum_cull_dset = VK_NULL_HANDLE;

	bool streamChunk(const terrain::Chunk &chunk, ChunkRenderInfo &rinfo);
	static bool renderInfoComparator(const ChunkRenderInfo &a, uint32_t lod_a, const ChunkRenderInfo &b,
		uint32_t lod_b) noexcept;
};

} // namespace voxen::client::vulkan
