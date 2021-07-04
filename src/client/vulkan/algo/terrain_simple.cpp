#include <voxen/client/vulkan/algo/terrain_simple.hpp>

#include <voxen/client/vulkan/high/terrain_synchronizer.hpp>
#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/pipeline.hpp>
#include <voxen/client/vulkan/pipeline_layout.hpp>
#include <voxen/common/terrain/surface.hpp>
#include <voxen/util/log.hpp>

#include <extras/math.hpp>

#include <glm/gtc/type_ptr.hpp>

namespace voxen::client::vulkan
{

struct PushConstantsBlock {
	float mtx[16];
	float sun_dir[3];
};

AlgoTerrainSimple::AlgoTerrainSimple()
{
	Log::debug("AlgoTerrainSimple created successfully");
}

void AlgoTerrainSimple::executePass(VkCommandBuffer cmd_buffer, const WorldState &state, const GameView &view)
{
	auto &backend = Backend::backend();
	auto &pipeline_layout_collection = backend.pipelineLayoutCollection();
	auto &pipeline_collection = backend.pipelineCollection();
	VkPipeline pipeline = pipeline_collection[PipelineCollection::TERRAIN_SIMPLE_PIPELINE];
	VkPipelineLayout pipeline_layout = pipeline_layout_collection.descriptorlessLayout();

	auto &terrain = backend.terrainSynchronizer();
	{
		terrain.beginSyncSession();
		state.walkActiveChunks([&](const terrain::Chunk &chunk) {
			if (isChunkVisible(chunk, view)) {
				terrain.syncChunk(chunk);
			}
		});
		terrain.endSyncSession();
	}

	backend.vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	static const glm::vec3 SUN_DIR = glm::normalize(glm::vec3(0.3f, 0.7f, 0.3f));

	auto view_proj_mat = view.cameraMatrix();

	terrain.walkActiveChunks(
	[&](terrain::ChunkId id, const TerrainChunkGpuData &data) {
		float base_x = float(id.base_x * int32_t(terrain::Config::CHUNK_SIZE));
		float base_y = float(id.base_y * int32_t(terrain::Config::CHUNK_SIZE));
		float base_z = float(id.base_z * int32_t(terrain::Config::CHUNK_SIZE));
		float size = float(1u << id.lod);
		glm::mat4 model_mat = extras::scale_translate(base_x, base_y, base_z, size);
		glm::mat4 mat = view_proj_mat * model_mat;

		PushConstantsBlock block;
		memcpy(block.mtx, glm::value_ptr(mat), sizeof(float) * 16);
		memcpy(block.sun_dir, glm::value_ptr(SUN_DIR), sizeof(float) * 3);
		backend.vkCmdPushConstants(cmd_buffer, pipeline_layout,
		                           VK_SHADER_STAGE_ALL,
		                           0, sizeof(block), &block);
		backend.vkCmdDrawIndexed(cmd_buffer, data.index_count, 1, data.first_index, data.vertex_offset, 0);
	},
	[&](VkBuffer vtx_buf, VkBuffer idx_buf) {
		VkDeviceSize offset = 0;
		backend.vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &vtx_buf, &offset);
		backend.vkCmdBindIndexBuffer(cmd_buffer, idx_buf, 0, VK_INDEX_TYPE_UINT32);
	});
}

bool AlgoTerrainSimple::isChunkVisible(const terrain::Chunk &chunk, const GameView &view) noexcept
{
	const auto &id = chunk.id();
	const auto &surface = chunk.ownSurface();
	const auto &seam_surface = chunk.seamSurface();

	if (surface.numIndices() == 0 && seam_surface.numIndices() == 0) {
		return false;
	}

	float base_x = float(id.base_x * terrain::Config::CHUNK_SIZE);
	float base_y = float(id.base_y * terrain::Config::CHUNK_SIZE);
	float base_z = float(id.base_z * terrain::Config::CHUNK_SIZE);
	float scale = float(1u << id.lod);
	glm::mat4 mat = view.cameraMatrix() * extras::scale_translate(base_x, base_y, base_z, scale);

	const Aabb &aabb = seam_surface.aabb();
	const glm::vec3 &aabb_min = aabb.min();
	const glm::vec3 &aabb_max = aabb.max();

	for (int i = 0; i < 8; i++) {
		glm::vec4 point;
		point.x = (i & 1) ? aabb_min.x : aabb_max.x;
		point.y = (i & 2) ? aabb_min.y : aabb_max.y;
		point.z = (i & 4) ? aabb_min.z : aabb_max.z;
		point.w = 1.0f;

		glm::vec4 ndc = mat * point;
		ndc /= ndc.w;
		if (ndc.x >= -1.0f && ndc.x <= 1.0f) {
			return true;
		}
		if (ndc.y >= -1.0f && ndc.y <= 1.0f) {
			return true;
		}
		if (ndc.z >= 0.0f && ndc.z <= 1.0f) {
			return true;
		}
	}

	return false;
}

}
