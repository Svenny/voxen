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

static glm::mat4 chunkLocalToClip(terrain::ChunkId id, const GameView &view) noexcept
{
	glm::dvec3 base_pos;
	base_pos.x = double(id.base_x * int32_t(terrain::Config::CHUNK_SIZE));
	base_pos.y = double(id.base_y * int32_t(terrain::Config::CHUNK_SIZE));
	base_pos.z = double(id.base_z * int32_t(terrain::Config::CHUNK_SIZE));

	const glm::vec3 offset(base_pos - view.cameraPosition());
	const float size = float(1u << id.lod);
	const glm::mat4 local_to_tr_world = extras::scale_translate(offset.x, offset.y, offset.z, size);
	return view.translatedWorldToClip() * local_to_tr_world;
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

	terrain.walkActiveChunks(
	[&](terrain::ChunkId id, const TerrainChunkGpuData &data) {
		const glm::mat4 mat = chunkLocalToClip(id, view);

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

	const glm::mat4 mat = chunkLocalToClip(id, view);

	const Aabb &aabb = seam_surface.aabb();
	const glm::vec3 &aabb_min = aabb.min();
	const glm::vec3 &aabb_max = aabb.max();

	const glm::vec4 partial_ndc_min[3] = {
		mat[0] * aabb_min.x, mat[1] * aabb_min.y, mat[2] * aabb_min.z
	};
	const glm::vec4 partial_ndc_max[3] = {
		mat[0] * aabb_max.x, mat[1] * aabb_max.y, mat[2] * aabb_max.z
	};
	const glm::vec4 partial_ndc_w = mat[3];

	glm::vec3 ndc_points[8];

	for (int i = 0; i < 8; i++) {
		glm::vec4 ndc = partial_ndc_w;
		ndc += (i & 1) ? partial_ndc_min[0] : partial_ndc_max[0];
		ndc += (i & 2) ? partial_ndc_min[1] : partial_ndc_max[1];
		ndc += (i & 4) ? partial_ndc_min[2] : partial_ndc_max[2];
		ndc_points[i] = glm::vec3(ndc) / ndc.w;
	}

	bool inside;

#define CHECK(expr) \
	inside = false; \
	for (int i = 0; i < 8; i++) { \
		if (ndc_points[i].expr) { \
			inside = true; \
			break; \
		} \
	} \
	if (!inside) { \
		return false; \
	}

	CHECK(z >= 0.0f);
	CHECK(z <= 1.0f);
	CHECK(x >= -1.0f);
	CHECK(x <= 1.0f);
	CHECK(y >= -1.0f);
	CHECK(y <= 1.0f);

#undef CHECK

	return true;
}

}
