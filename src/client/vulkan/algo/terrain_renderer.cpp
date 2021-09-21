#include <voxen/client/vulkan/algo/terrain_renderer.hpp>

#include <voxen/client/vulkan/high/terrain_synchronizer.hpp>
#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/descriptor_manager.hpp>
#include <voxen/client/vulkan/device.hpp>
#include <voxen/client/vulkan/pipeline.hpp>
#include <voxen/client/vulkan/pipeline_layout.hpp>
#include <voxen/common/gameview.hpp>
#include <voxen/common/world_state.hpp>
#include <voxen/common/terrain/chunk.hpp>
#include <voxen/common/terrain/config.hpp>
#include <voxen/common/terrain/surface.hpp>
#include <voxen/util/log.hpp>

#include <extras/math.hpp>

namespace voxen::client::vulkan
{

constexpr static size_t MAX_PER_ARENA_VERTICES = 1024 * 1024;
constexpr static size_t MAX_PER_ARENA_INDICES = 6 * MAX_PER_ARENA_VERTICES;
constexpr static size_t MAX_RENDERED_CHUNKS = 2048;

constexpr VkDeviceSize CHUNK_TRANSFORM_BUFFER_SIZE = sizeof(glm::vec4) * MAX_RENDERED_CHUNKS;
constexpr VkDeviceSize DRAW_COMMAND_BUFFER_SIZE = sizeof(VkDrawIndexedIndirectCommand) * MAX_RENDERED_CHUNKS;
constexpr VkDeviceSize CHUNK_AABB_BUFFER_SIZE = sizeof(Aabb) * MAX_RENDERED_CHUNKS;
constexpr VkDeviceSize VERTEX_ARENA_SIZE = sizeof(terrain::SurfaceVertex) * MAX_PER_ARENA_VERTICES;
constexpr VkDeviceSize INDEX_ARENA_SIZE = sizeof(uint16_t) * MAX_PER_ARENA_INDICES;

constexpr static float DEBUG_OCTREE_VERTEX_BUFFER_DATA[] = {
#define LO 0.0f
#define HI float(terrain::Config::CHUNK_SIZE)
	LO, LO, LO,
	LO, LO, HI,
	LO, HI, LO,
	LO, HI, HI,
	HI, LO, LO,
	HI, LO, HI,
	HI, HI, LO,
	HI, HI, HI
#undef LO
#undef HI
};

constexpr static uint16_t DEBUG_OCTREE_INDEX_BUFFER_DATA[] = {
	0, 1, 1, 5, 4, 5, 0, 4,
	2, 3, 3, 7, 6, 7, 2, 6,
	0, 2, 1, 3, 4, 6, 5, 7
};

TerrainRenderer::TerrainRenderer()
{
	constexpr VkDeviceSize per_frame_size =
		CHUNK_TRANSFORM_BUFFER_SIZE + DRAW_COMMAND_BUFFER_SIZE + CHUNK_AABB_BUFFER_SIZE;
	constexpr VkDeviceSize combo_size = per_frame_size * Config::NUM_CPU_PENDING_FRAMES;

	constexpr VkBufferCreateInfo combo_info {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.size = combo_size,
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
			VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr
	};
	m_combo_buffer = WrappedVkBuffer(combo_info);

	auto &allocator = Backend::backend().deviceAllocator();
	m_combo_buffer_memory = allocator.allocate(m_combo_buffer, DeviceMemoryUseCase::FastUpload);
	m_combo_buffer.bindMemory(m_combo_buffer_memory.handle(), m_combo_buffer_memory.offset());

	m_combo_buffer_host_ptr = m_combo_buffer_memory.tryHostMap();
	// `FastUpload` use case guarantees host visiblity
	assert(m_combo_buffer_host_ptr);

	for (uint32_t i = 0; i < Config::NUM_CPU_PENDING_FRAMES; i++) {
		uintptr_t base = uintptr_t(m_combo_buffer_host_ptr) + per_frame_size * i;

		m_chunk_transform_ptr[i] = reinterpret_cast<glm::vec4 *>(base);
		m_draw_command_ptr[i] = reinterpret_cast<VkDrawIndexedIndirectCommand *>(base + CHUNK_TRANSFORM_BUFFER_SIZE);
		m_chunk_aabb_ptr[i] = reinterpret_cast<Aabb *>(base + CHUNK_TRANSFORM_BUFFER_SIZE + DRAW_COMMAND_BUFFER_SIZE);
	}

	Log::debug("TerrainRenderer created successfully");
}

void TerrainRenderer::onNewWorldState(const WorldState &state)
{
	// TODO: this does not prolong lifetime, will lead to dangling
	// pointer if world state is not updated every frame
	m_last_state = &state;
}

static glm::vec4 calcChunkBaseScale(terrain::ChunkId id, const GameView &view) noexcept
{
	glm::dvec3 base_pos;
	base_pos.x = double(id.base_x * int32_t(terrain::Config::CHUNK_SIZE));
	base_pos.y = double(id.base_y * int32_t(terrain::Config::CHUNK_SIZE));
	base_pos.z = double(id.base_z * int32_t(terrain::Config::CHUNK_SIZE));

	const float size = float(1u << id.lod);
	return glm::vec4(base_pos - view.cameraPosition(), size);
}

void TerrainRenderer::onFrameBegin(const GameView &view)
{
	auto &backend = Backend::backend();

	uint32_t set_id = backend.descriptorManager().setId();
	VkDescriptorBufferInfo buffer_infos[3];
	buffer_infos[0] = {
		.buffer = m_combo_buffer,
		.offset = uintptr_t(m_chunk_transform_ptr[set_id]) - uintptr_t(m_combo_buffer_host_ptr),
		.range = CHUNK_TRANSFORM_BUFFER_SIZE
	};
	buffer_infos[1] = {
		.buffer = m_combo_buffer,
		.offset = uintptr_t(m_draw_command_ptr[set_id]) - uintptr_t(m_combo_buffer_host_ptr),
		.range = DRAW_COMMAND_BUFFER_SIZE
	};
	buffer_infos[2] = {
		.buffer = m_combo_buffer,
		.offset = uintptr_t(m_chunk_aabb_ptr[set_id]) - uintptr_t(m_combo_buffer_host_ptr),
		.range = CHUNK_AABB_BUFFER_SIZE
	};

	VkDescriptorSet frustum_cull_set = backend.descriptorManager().terrainFrustumCullSet();
	VkWriteDescriptorSet writes[3];
	writes[0] = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.pNext = nullptr,
		.dstSet = frustum_cull_set,
		.dstBinding = 0,
		.dstArrayElement = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.pImageInfo = nullptr,
		.pBufferInfo = &buffer_infos[0],
		.pTexelBufferView = nullptr
	};
	writes[1] = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.pNext = nullptr,
		.dstSet = frustum_cull_set,
		.dstBinding = 1,
		.dstArrayElement = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.pImageInfo = nullptr,
		.pBufferInfo = &buffer_infos[1],
		.pTexelBufferView = nullptr
	};
	writes[2] = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.pNext = nullptr,
		.dstSet = frustum_cull_set,
		.dstBinding = 2,
		.dstArrayElement = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.pImageInfo = nullptr,
		.pBufferInfo = &buffer_infos[2],
		.pTexelBufferView = nullptr
	};

	backend.vkUpdateDescriptorSets(backend.device(), std::size(writes), writes, 0, nullptr);

	auto &terrain_sync = backend.terrainSynchronizer();
	terrain_sync.beginSyncSession();

	m_num_active_chunks = 0;
	m_last_state->walkActiveChunks([this, &view, &terrain_sync, set_id](const terrain::Chunk &chunk) {
		const terrain::ChunkOwnSurface &own_surf = chunk.ownSurface();
		const terrain::ChunkSeamSurface &seam_surf = chunk.seamSurface();
		if (own_surf.numIndices() == 0 && seam_surf.numIndices() == 0) {
			return;
		}

		terrain_sync.syncChunk(chunk);
		m_kek_tmp[chunk.id()] = m_num_active_chunks;

		m_chunk_transform_ptr[set_id][m_num_active_chunks] = calcChunkBaseScale(chunk.id(), view);
		m_draw_command_ptr[set_id][m_num_active_chunks] = VkDrawIndexedIndirectCommand {
			.indexCount = own_surf.numIndices() + seam_surf.numIndices(),
			.instanceCount = 1,
			.firstIndex = 0,
			.vertexOffset = 0,
			.firstInstance = m_num_active_chunks
		};
		m_chunk_aabb_ptr[set_id][m_num_active_chunks] = seam_surf.aabb();
		m_num_active_chunks++;
	});

	terrain_sync.endSyncSession();
}

void TerrainRenderer::prepareResources(VkCommandBuffer cmdbuf)
{
	auto &backend = Backend::backend();

	// TODO: check some debug flag to avoid creating this buffer when it's not needed?
	if (m_debug_octree_mesh_buffer.handle() == VK_NULL_HANDLE) {
		constexpr VkDeviceSize size = sizeof(DEBUG_OCTREE_VERTEX_BUFFER_DATA) + sizeof(DEBUG_OCTREE_INDEX_BUFFER_DATA);
		// TODO: Not efficient and generally looks so GL-ish
		m_debug_octree_mesh_buffer = FatVkBuffer(VkBufferCreateInfo {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.size = size,
			.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices = nullptr
		}, DeviceMemoryUseCase::GpuOnly);

		backend.vkCmdUpdateBuffer(cmdbuf, m_debug_octree_mesh_buffer, 0,
			sizeof(DEBUG_OCTREE_VERTEX_BUFFER_DATA), DEBUG_OCTREE_VERTEX_BUFFER_DATA);
		backend.vkCmdUpdateBuffer(cmdbuf, m_debug_octree_mesh_buffer, sizeof(DEBUG_OCTREE_VERTEX_BUFFER_DATA),
			sizeof(DEBUG_OCTREE_INDEX_BUFFER_DATA), DEBUG_OCTREE_INDEX_BUFFER_DATA);
	}
}

void TerrainRenderer::launchFrustumCull(VkCommandBuffer cmdbuf)
{
	auto &backend = Backend::backend();

	VkPipeline pipe = backend.pipelineCollection()[PipelineCollection::TERRAIN_FRUSTUM_CULL_PIPELINE];
	backend.vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);

	VkPipelineLayout pipe_layout = backend.pipelineLayoutCollection().terrainFrustumCullLayout();

	VkDescriptorSet sets[2];
	sets[0] = backend.descriptorManager().mainSceneSet();
	sets[1] = backend.descriptorManager().terrainFrustumCullSet();
	backend.vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipe_layout,
		0, std::size(sets), sets, 0, nullptr);

	backend.vkCmdDispatch(cmdbuf, (m_num_active_chunks + 63) / 64, 1, 1);

	uint32_t set_id = backend.descriptorManager().setId();
	const VkBufferMemoryBarrier barrier {
		.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		.pNext = nullptr,
		.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.buffer = m_combo_buffer,
		.offset = uintptr_t(m_draw_command_ptr[set_id]) - uintptr_t(m_combo_buffer_host_ptr),
		.size = DRAW_COMMAND_BUFFER_SIZE
	};
	backend.vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
		0, 0, nullptr, 1, &barrier, 0, nullptr);
}

void TerrainRenderer::drawChunksInFrustum(VkCommandBuffer cmdbuf)
{
	auto &backend = Backend::backend();
	auto &pipeline_layout_collection = backend.pipelineLayoutCollection();
	auto &pipeline_collection = backend.pipelineCollection();

	VkPipeline pipeline = pipeline_collection[PipelineCollection::TERRAIN_SIMPLE_PIPELINE];
	VkPipelineLayout pipeline_layout = pipeline_layout_collection.terrainBasicLayout();
	VkDescriptorSet descriptor_set = backend.descriptorManager().mainSceneSet();

	backend.vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	backend.vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
		0, 1, &descriptor_set, 0, nullptr);

	static const glm::vec3 SUN_DIR = glm::normalize(glm::vec3(0.3f, 0.7f, 0.3f));
	backend.vkCmdPushConstants(cmdbuf, pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SUN_DIR), &SUN_DIR);

	const uint32_t set_id = backend.descriptorManager().setId();
	VkBuffer const xfm_buffer = m_combo_buffer;
	const VkDeviceSize xfm_offset = uintptr_t(m_chunk_transform_ptr[set_id]) - uintptr_t(m_combo_buffer_host_ptr);
	backend.vkCmdBindVertexBuffers(cmdbuf, 1, 1, &xfm_buffer, &xfm_offset);

	backend.terrainSynchronizer().walkActiveChunks(
	[&](terrain::ChunkId id, const TerrainChunkGpuData &/*data*/) {
		VkDeviceSize draw_offset = uintptr_t(m_draw_command_ptr[set_id]) - uintptr_t(m_combo_buffer_host_ptr);
		draw_offset += sizeof(VkDrawIndexedIndirectCommand) * m_kek_tmp[id];
		backend.vkCmdDrawIndexedIndirect(cmdbuf, m_combo_buffer, draw_offset, 1, sizeof(VkDrawIndexedIndirectCommand));
	},
	[&](VkBuffer vtx_buf, VkBuffer idx_buf) {
		VkDeviceSize offset = 0;
		backend.vkCmdBindVertexBuffers(cmdbuf, 0, 1, &vtx_buf, &offset);
		backend.vkCmdBindIndexBuffer(cmdbuf, idx_buf, 0, VK_INDEX_TYPE_UINT32);
	});
}

void TerrainRenderer::drawDebugChunkBorders(VkCommandBuffer cmdbuf)
{
	auto &backend = Backend::backend();
	auto &pipeline_layout_collection = backend.pipelineLayoutCollection();
	auto &pipeline_collection = backend.pipelineCollection();

	VkPipeline pipeline = pipeline_collection[PipelineCollection::DEBUG_OCTREE_PIPELINE];
	VkPipelineLayout pipeline_layout = pipeline_layout_collection.terrainBasicLayout();
	VkDescriptorSet descriptor_set = backend.descriptorManager().mainSceneSet();

	backend.vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	backend.vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
		0, 1, &descriptor_set, 0, nullptr);

	const uint32_t set_id = backend.descriptorManager().setId();
	VkBuffer vtx_buffers[2];
	vtx_buffers[0] = m_debug_octree_mesh_buffer;
	vtx_buffers[1] = m_combo_buffer;
	VkDeviceSize vtx_offsets[2];
	vtx_offsets[0] = 0;
	vtx_offsets[1] = uintptr_t(m_chunk_transform_ptr[set_id]) - uintptr_t(m_combo_buffer_host_ptr);

	backend.vkCmdBindVertexBuffers(cmdbuf, 0, std::size(vtx_buffers), vtx_buffers, vtx_offsets);
	backend.vkCmdBindIndexBuffer(cmdbuf, m_debug_octree_mesh_buffer,
		sizeof(DEBUG_OCTREE_VERTEX_BUFFER_DATA), VK_INDEX_TYPE_UINT16);
	backend.vkCmdDrawIndexed(cmdbuf, std::size(DEBUG_OCTREE_INDEX_BUFFER_DATA), m_num_active_chunks, 0, 0, 0);
}

void TerrainRenderer::addVertexArena()
{
	constexpr VkBufferCreateInfo info {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.size = VERTEX_ARENA_SIZE,
		.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr
	};
	m_vertex_arenas.emplace_back(FatVkBuffer(info, DeviceMemoryUseCase::GpuOnly));
}

void TerrainRenderer::addIndexArena()
{
	constexpr VkBufferCreateInfo info {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.size = INDEX_ARENA_SIZE,
		.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr
	};
	m_index_arenas.emplace_back(FatVkBuffer(info, DeviceMemoryUseCase::GpuOnly));
}

}
