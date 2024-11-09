#include <voxen/client/vulkan/algo/terrain_renderer.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/high/terrain_synchronizer.hpp>
#include <voxen/client/vulkan/pipeline.hpp>
#include <voxen/client/vulkan/pipeline_layout.hpp>
#include <voxen/common/gameview.hpp>
#include <voxen/common/terrain/chunk.hpp>
#include <voxen/common/terrain/config.hpp>
#include <voxen/common/terrain/surface.hpp>
#include <voxen/common/world_state.hpp>
#include <voxen/gfx/vk/vk_device.hpp>
#include <voxen/util/log.hpp>

#include <extras/dyn_array.hpp>
#include <extras/math.hpp>

namespace voxen::client::vulkan
{

constexpr VkDeviceSize CHUNK_TRANSFORM_BUFFER_SIZE = sizeof(glm::vec4) * Config::MAX_RENDERED_CHUNKS;
constexpr VkDeviceSize DRAW_COMMAND_BUFFER_SIZE = sizeof(VkDrawIndexedIndirectCommand) * Config::MAX_RENDERED_CHUNKS;
constexpr VkDeviceSize CHUNK_AABB_BUFFER_SIZE = sizeof(Aabb) * Config::MAX_RENDERED_CHUNKS;

constexpr VkDeviceSize COMBO_BUFFER_PER_FRAME_SIZE = CHUNK_TRANSFORM_BUFFER_SIZE + DRAW_COMMAND_BUFFER_SIZE
	+ CHUNK_AABB_BUFFER_SIZE;
constexpr VkDeviceSize COMBO_BUFFER_SIZE = COMBO_BUFFER_PER_FRAME_SIZE * Config::NUM_CPU_PENDING_FRAMES;

// clang-format off: breaks more readable table formatting
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
// clang-format on

TerrainRenderer::TerrainRenderer()
{
	constexpr VkBufferCreateInfo combo_info {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.size = COMBO_BUFFER_SIZE,
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
			| VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
	};
	m_combo_buffer = FatVkBuffer(combo_info, FatVkBuffer::Usage::Staging);

	m_combo_buffer_host_ptr = m_combo_buffer.hostPointer();
	// Upload case guarantees host visiblity
	assert(m_combo_buffer_host_ptr);

	for (uint32_t i = 0; i < Config::NUM_CPU_PENDING_FRAMES; i++) {
		std::byte *base = reinterpret_cast<std::byte *>(m_combo_buffer_host_ptr) + COMBO_BUFFER_PER_FRAME_SIZE * i;

		m_chunk_transform_ptr[i] = reinterpret_cast<glm::vec4 *>(base);
		m_draw_command_ptr[i] = reinterpret_cast<VkDrawIndexedIndirectCommand *>(base + CHUNK_TRANSFORM_BUFFER_SIZE);
		m_chunk_aabb_ptr[i] = reinterpret_cast<Aabb *>(base + CHUNK_TRANSFORM_BUFFER_SIZE + DRAW_COMMAND_BUFFER_SIZE);
	}

	// It's entirely unlikely that we can reach even 10
	m_draw_setups.reserve(10);

	Log::debug("TerrainRenderer created successfully");
}

VkDeviceSize TerrainRenderer::getComboBufferSize() const noexcept
{
	return COMBO_BUFFER_SIZE;
}

void TerrainRenderer::onNewWorldState(const WorldState &state)
{
	// TODO: this does not prolong lifetime, will lead to dangling
	// pointer if world state is not updated every frame
	m_last_state = &state;
}

static glm::vec4 calcChunkBaseScale(land::ChunkKey id, const GameView &view) noexcept
{
	glm::dvec3 base_pos;
	base_pos.x = double(id.x * int32_t(terrain::Config::CHUNK_SIZE));
	base_pos.y = double(id.y * int32_t(terrain::Config::CHUNK_SIZE));
	base_pos.z = double(id.z * int32_t(terrain::Config::CHUNK_SIZE));

	const float size = float(1u << id.scale_log2);
	return glm::vec4(base_pos - view.cameraPosition(), size);
}

static bool renderInfoComparator(const TerrainSynchronizer::ChunkRenderInfo &a, uint32_t lod_a,
	const TerrainSynchronizer::ChunkRenderInfo &b, uint32_t lod_b) noexcept
{
	// Using the following composite ordering:
	// - First compare index types (to minimize index stream type switching)
	// - Then compare vertex buffer handles (to minimize vertex streams switching)
	// - Then compare index buffer handles (to minimize index stream switching)
	// - Then sort by chunk LODs, smaller going first (to hopefully minimize overdraw by drawing near to far)
	// - Finally sort by first index to make ordering consistent (they can't be equal)

	const VkIndexType type_a = a.index_type;
	const VkIndexType type_b = b.index_type;
	if (type_a != type_b) {
		// U8 goes before U16 and U32
		if (type_a == VK_INDEX_TYPE_UINT8_EXT) {
			return true;
		}
		if (type_b == VK_INDEX_TYPE_UINT8_EXT) {
			return false;
		}
		// Two possible cases left:
		// `A == U16 && B == U32` (A goes before B) or
		// `A == U32 && B == U16` (B goes before A)
		return type_a == VK_INDEX_TYPE_UINT16;
	}

	if (a.vertex_buffer != b.vertex_buffer) {
		return a.vertex_buffer < b.vertex_buffer;
	}

	if (a.index_buffer != b.index_buffer) {
		return a.index_buffer < b.index_buffer;
	}

	if (lod_a != lod_b) {
		return lod_a < lod_b;
	}

	return a.first_index < b.first_index;
}

void TerrainRenderer::onFrameBegin(const GameView &view, VkDescriptorSet main_scene_dset,
	VkDescriptorSet frustum_cull_dset)
{
	m_combo_buffer_active_section = (m_combo_buffer_active_section + 1) % Config::NUM_CPU_PENDING_FRAMES;
	m_main_scene_dset = main_scene_dset;
	m_frustum_cull_dset = frustum_cull_dset;

	using RenderInfo = TerrainSynchronizer::ChunkRenderInfo;

	auto &backend = Backend::backend();

	uint32_t set_id = m_combo_buffer_active_section;
	VkDescriptorBufferInfo buffer_infos[3];
	buffer_infos[0] = {
		.buffer = m_combo_buffer,
		.offset = uintptr_t(m_chunk_transform_ptr[set_id]) - uintptr_t(m_combo_buffer_host_ptr),
		.range = CHUNK_TRANSFORM_BUFFER_SIZE,
	};
	buffer_infos[1] = {
		.buffer = m_combo_buffer,
		.offset = uintptr_t(m_draw_command_ptr[set_id]) - uintptr_t(m_combo_buffer_host_ptr),
		.range = DRAW_COMMAND_BUFFER_SIZE,
	};
	buffer_infos[2] = {
		.buffer = m_combo_buffer,
		.offset = uintptr_t(m_chunk_aabb_ptr[set_id]) - uintptr_t(m_combo_buffer_host_ptr),
		.range = CHUNK_AABB_BUFFER_SIZE,
	};

	VkDescriptorSet frustum_cull_set = m_frustum_cull_dset;
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
		.pTexelBufferView = nullptr,
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
		.pTexelBufferView = nullptr,
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
		.pTexelBufferView = nullptr,
	};

	backend.vkUpdateDescriptorSets(backend.device().handle(), std::size(writes), writes, 0, nullptr);

	auto &terrain_sync = backend.terrainSynchronizer();
	terrain_sync.beginSyncSession();

	extras::dyn_array<std::pair<const terrain::Chunk *, RenderInfo>> render_infos(Config::MAX_RENDERED_CHUNKS);

	m_num_active_chunks = 0;
	m_last_state->walkActiveChunksPointers([&](const ChunkPtr &chunk) {
		if (!chunk->hasSurface()) {
			return;
		}

		render_infos[m_num_active_chunks] = { chunk.get(), terrain_sync.syncChunk(chunk) };
		m_num_active_chunks++;
	});

	auto last_info = render_infos.begin() + m_num_active_chunks;
	std::sort(render_infos.begin(), last_info, [&](const auto &a, const auto &b) {
		return renderInfoComparator(a.second, a.first->id().scale_log2, b.second, b.first->id().scale_log2);
	});

	m_draw_setups.clear();
	if (m_num_active_chunks != 0) {
		const auto &first_info = render_infos[0].second;
		m_draw_setups.emplace_back(0, first_info.vertex_buffer, first_info.index_buffer, first_info.index_type);
	}

	for (uint32_t i = 0; i < m_num_active_chunks; i++) {
		const terrain::Chunk &chunk = *render_infos[i].first;
		const auto &render_info = render_infos[i].second;

		const auto &last_draw_setup = m_draw_setups.back();
		if (std::get<1>(last_draw_setup) != render_info.vertex_buffer
			|| std::get<2>(last_draw_setup) != render_info.index_buffer
			|| std::get<3>(last_draw_setup) != render_info.index_type) {
			m_draw_setups.emplace_back(i, render_info.vertex_buffer, render_info.index_buffer, render_info.index_type);
		}

		// TODO (Svenny): base/scale calculations can be easily vectorized
		m_chunk_transform_ptr[set_id][i] = calcChunkBaseScale(chunk.id(), view);
		m_draw_command_ptr[set_id][i] = VkDrawIndexedIndirectCommand {
			.indexCount = render_info.num_indices,
			.instanceCount = 1,
			.firstIndex = render_info.first_index,
			.vertexOffset = render_info.first_vertex,
			.firstInstance = i,
		};
		m_chunk_aabb_ptr[set_id][i] = chunk.surface().aabb();
	}

	terrain_sync.endSyncSession();
}

void TerrainRenderer::prepareResources(VkCommandBuffer cmdbuf)
{
	auto &backend = Backend::backend();

	// TODO: check some debug flag to avoid creating this buffer when it's not needed?
	if (m_debug_octree_mesh_buffer.handle() == VK_NULL_HANDLE) {
		constexpr VkDeviceSize size = sizeof(DEBUG_OCTREE_VERTEX_BUFFER_DATA) + sizeof(DEBUG_OCTREE_INDEX_BUFFER_DATA);
		// TODO: Not efficient and generally looks so GL-ish
		m_debug_octree_mesh_buffer = FatVkBuffer(
			VkBufferCreateInfo {
				.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.size = size,
				.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
					| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
				.queueFamilyIndexCount = 0,
				.pQueueFamilyIndices = nullptr,
			},
			FatVkBuffer::Usage::DeviceLocal);

		backend.vkCmdUpdateBuffer(cmdbuf, m_debug_octree_mesh_buffer, 0, sizeof(DEBUG_OCTREE_VERTEX_BUFFER_DATA),
			DEBUG_OCTREE_VERTEX_BUFFER_DATA);
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
	sets[0] = m_main_scene_dset;
	sets[1] = m_frustum_cull_dset;
	backend.vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipe_layout, 0, std::size(sets), sets, 0,
		nullptr);

	backend.vkCmdDispatch(cmdbuf, (m_num_active_chunks + 63) / 64, 1, 1);

	uint32_t set_id = m_combo_buffer_active_section;
	const VkBufferMemoryBarrier2 barrier {
		.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
		.pNext = nullptr,
		.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
		.dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.buffer = m_combo_buffer,
		.offset = uintptr_t(m_draw_command_ptr[set_id]) - uintptr_t(m_combo_buffer_host_ptr),
		.size = DRAW_COMMAND_BUFFER_SIZE,
	};

	const VkDependencyInfo depInfo {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.pNext = nullptr,
		.dependencyFlags = 0,
		.memoryBarrierCount = 0,
		.pMemoryBarriers = nullptr,
		.bufferMemoryBarrierCount = 1,
		.pBufferMemoryBarriers = &barrier,
		.imageMemoryBarrierCount = 0,
		.pImageMemoryBarriers = nullptr,
	};

	backend.vkCmdPipelineBarrier2(cmdbuf, &depInfo);
}

void TerrainRenderer::drawChunksInFrustum(VkCommandBuffer cmdbuf)
{
	auto &backend = Backend::backend();
	auto &pipeline_layout_collection = backend.pipelineLayoutCollection();
	auto &pipeline_collection = backend.pipelineCollection();

	VkPipeline pipeline = pipeline_collection[PipelineCollection::TERRAIN_SIMPLE_PIPELINE];
	VkPipelineLayout pipeline_layout = pipeline_layout_collection.terrainBasicLayout();
	VkDescriptorSet descriptor_set = m_main_scene_dset;

	backend.vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	backend.vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0,
		nullptr);

	static const glm::vec3 SUN_DIR = glm::normalize(glm::vec3(0.3f, 0.7f, 0.3f));
	backend.vkCmdPushConstants(cmdbuf, pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SUN_DIR), &SUN_DIR);

	const uint32_t set_id = m_combo_buffer_active_section;
	VkBuffer const xfm_buffer = m_combo_buffer;
	const VkDeviceSize xfm_offset = uintptr_t(m_chunk_transform_ptr[set_id]) - uintptr_t(m_combo_buffer_host_ptr);
	backend.vkCmdBindVertexBuffers(cmdbuf, 1, 1, &xfm_buffer, &xfm_offset);

	for (auto iter = m_draw_setups.begin(); iter != m_draw_setups.end(); ++iter) {
		const auto &cur_setup = *iter;
		uint32_t first_draw = std::get<0>(cur_setup);
		VkBuffer vertex_buffer = std::get<1>(cur_setup);
		VkBuffer index_buffer = std::get<2>(cur_setup);
		VkIndexType index_type = std::get<3>(cur_setup);

		const VkDeviceSize offset = 0;
		backend.vkCmdBindVertexBuffers(cmdbuf, 0, 1, &vertex_buffer, &offset);
		backend.vkCmdBindIndexBuffer(cmdbuf, index_buffer, 0, index_type);

		VkDeviceSize draw_offset = uintptr_t(m_draw_command_ptr[set_id]) - uintptr_t(m_combo_buffer_host_ptr);
		draw_offset += sizeof(VkDrawIndexedIndirectCommand) * first_draw;

		uint32_t draw_count;
		if (iter + 1 != m_draw_setups.end()) {
			const auto &next_setup = *(iter + 1);
			draw_count = std::get<0>(next_setup) - first_draw;
		} else {
			draw_count = m_num_active_chunks - first_draw;
		}

		backend.vkCmdDrawIndexedIndirect(cmdbuf, m_combo_buffer, draw_offset, draw_count,
			sizeof(VkDrawIndexedIndirectCommand));
	}
}

void TerrainRenderer::drawDebugChunkBorders(VkCommandBuffer cmdbuf)
{
	auto &backend = Backend::backend();
	auto &pipeline_layout_collection = backend.pipelineLayoutCollection();
	auto &pipeline_collection = backend.pipelineCollection();

	VkPipeline pipeline = pipeline_collection[PipelineCollection::DEBUG_OCTREE_PIPELINE];
	VkPipelineLayout pipeline_layout = pipeline_layout_collection.terrainBasicLayout();
	VkDescriptorSet descriptor_set = m_main_scene_dset;

	backend.vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	backend.vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0,
		nullptr);

	const uint32_t set_id = m_combo_buffer_active_section;
	VkBuffer vtx_buffers[2];
	vtx_buffers[0] = m_debug_octree_mesh_buffer;
	vtx_buffers[1] = m_combo_buffer;
	VkDeviceSize vtx_offsets[2];
	vtx_offsets[0] = 0;
	vtx_offsets[1] = uintptr_t(m_chunk_transform_ptr[set_id]) - uintptr_t(m_combo_buffer_host_ptr);

	backend.vkCmdBindVertexBuffers(cmdbuf, 0, std::size(vtx_buffers), vtx_buffers, vtx_offsets);
	backend.vkCmdBindIndexBuffer(cmdbuf, m_debug_octree_mesh_buffer, sizeof(DEBUG_OCTREE_VERTEX_BUFFER_DATA),
		VK_INDEX_TYPE_UINT16);
	backend.vkCmdDrawIndexed(cmdbuf, std::size(DEBUG_OCTREE_INDEX_BUFFER_DATA), m_num_active_chunks, 0, 0, 0);
}

} // namespace voxen::client::vulkan
