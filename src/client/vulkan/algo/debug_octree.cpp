#include <voxen/client/vulkan/algo/debug_octree.hpp>

#include <voxen/client/vulkan/high/terrain_synchronizer.hpp>
#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/descriptor_manager.hpp>
#include <voxen/client/vulkan/pipeline.hpp>
#include <voxen/client/vulkan/pipeline_layout.hpp>

#include <voxen/util/log.hpp>

#include <extras/math.hpp>

#include <glm/gtc/type_ptr.hpp>

namespace voxen::client::vulkan
{

constexpr static float VERTEX_BUFFER_DATA[] = {
	0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f,
	0.0f, 1.0f, 0.0f,
	0.0f, 1.0f, 1.0f,
	1.0f, 0.0f, 0.0f,
	1.0f, 0.0f, 1.0f,
	1.0f, 1.0f, 0.0f,
	1.0f, 1.0f, 1.0f
};

constexpr static uint16_t INDEX_BUFFER_DATA[] = {
	0, 1, 1, 5, 4, 5, 0, 4,
	2, 3, 3, 7, 6, 7, 2, 6,
	0, 2, 1, 3, 4, 6, 5, 7
};

struct PushConstantsBlock {
	glm::vec4 chunk_base_scale;
	glm::vec4 debug_color;
};

AlgoDebugOctree::AlgoDebugOctree() :
	m_cell_mesh(createCellMesh())
{
	Log::debug("AlgoDebugOctree created successfully");
}

static glm::vec4 calcChunkBaseScale(terrain::ChunkId id, const GameView &view) noexcept
{
	glm::dvec3 base_pos;
	base_pos.x = double(id.base_x * int32_t(terrain::Config::CHUNK_SIZE));
	base_pos.y = double(id.base_y * int32_t(terrain::Config::CHUNK_SIZE));
	base_pos.z = double(id.base_z * int32_t(terrain::Config::CHUNK_SIZE));

	const float size = float(terrain::Config::CHUNK_SIZE << id.lod);
	return glm::vec4(base_pos - view.cameraPosition(), size);
}

void AlgoDebugOctree::executePass(VkCommandBuffer cmd_buffer, const GameView &view)
{
	auto &backend = Backend::backend();
	auto &pipeline_layout_collection = backend.pipelineLayoutCollection();
	auto &pipeline_collection = backend.pipelineCollection();
	VkPipeline pipeline = pipeline_collection[PipelineCollection::DEBUG_OCTREE_PIPELINE];
	VkPipelineLayout pipeline_layout = pipeline_layout_collection.terrainBasicLayout();
	VkDescriptorSet descriptor_set = backend.descriptorManager().mainSceneSet();

	backend.vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	backend.vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
	                                0, 1, &descriptor_set, 0, nullptr);
	m_cell_mesh.bindBuffers(cmd_buffer);

	static constexpr float NODE_COLORS[6][4] = {
		{ 1.0f, 0.0f, 0.0f, 1.0f },
		{ 0.8f, 0.2f, 0.0f, 1.0f },
		{ 0.6f, 0.4f, 0.0f, 1.0f },
		{ 0.4f, 0.6f, 0.0f, 1.0f },
		{ 0.2f, 0.8f, 0.0f, 1.0f },
		{ 0.0f, 1.0f, 0.0f, 1.0f }
	};

	auto &terrain = backend.terrainSynchronizer();
	terrain.walkActiveChunks(
	[&](terrain::ChunkId id, const TerrainChunkGpuData &data) {
		if (data.index_count == 0) {
			// Don't draw junk lines for empty chunks
			return;
		}

		const float size = float(terrain::Config::CHUNK_SIZE << id.lod);
		const int idx = std::clamp(int(log2f(size / 32.0f)), 0, 5);

		PushConstantsBlock block;
		block.chunk_base_scale = calcChunkBaseScale(id, view);
		block.debug_color.r = NODE_COLORS[idx][0];
		block.debug_color.g = NODE_COLORS[idx][1];
		block.debug_color.b = NODE_COLORS[idx][2];
		block.debug_color.a = NODE_COLORS[idx][3];
		backend.vkCmdPushConstants(cmd_buffer, pipeline_layout,
		                           VK_SHADER_STAGE_ALL,
		                           0, sizeof(block), &block);
		// TODO: Mesh must support drawing itself
		backend.vkCmdDrawIndexed(cmd_buffer, std::size(INDEX_BUFFER_DATA), 1, 0, 0, 0);
	},
	[&](VkBuffer /*vtx_buf*/, VkBuffer /*idx_buf*/) {
		// Do nothing - we use our own set of buffers
	});
}

Mesh AlgoDebugOctree::createCellMesh()
{
	MeshCreateInfo info = {};
	info.vertex_format = VertexFormat::Pos3D;
	info.num_vertices = std::size(VERTEX_BUFFER_DATA) / 3;
	info.vertex_data = VERTEX_BUFFER_DATA;
	info.index_format = IndexFormat::Index16;
	info.num_indices = std::size(INDEX_BUFFER_DATA);
	info.index_data = INDEX_BUFFER_DATA;
	return Mesh(info);
}

}
