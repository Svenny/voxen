#include <voxen/gfx/vk/legacy_render_graph.hpp>

#include <voxen/client/vulkan/algo/terrain_renderer.hpp>
#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/descriptor_set_layout.hpp>
#include <voxen/client/vulkan/pipeline.hpp>
#include <voxen/client/vulkan/pipeline_layout.hpp>
#include <voxen/gfx/font_renderer.hpp>
#include <voxen/gfx/gfx_land_loader.hpp>
#include <voxen/gfx/gfx_system.hpp>
#include <voxen/gfx/vk/frame_context.hpp>
#include <voxen/gfx/vk/render_graph_builder.hpp>
#include <voxen/gfx/vk/render_graph_execution.hpp>
#include <voxen/gfx/vk/vk_device.hpp>
#include <voxen/land/land_temp_blocks.hpp>

#include <cstring>

namespace voxen::gfx::vk
{

namespace
{

struct MainSceneUbo {
	glm::mat4 translated_world_to_clip;
	glm::vec3 world_position;
	float _pad0;
};

constexpr VkClearColorValue CLEAR_COLOR = { { 0.53f, 0.77f, 0.9f, 1.0f } };
constexpr VkClearDepthStencilValue CLEAR_DEPTH = { 0.0f, 0 };

// TODO: unify with shader code
struct PseudoSurfaceDrawCommand {
	VkDeviceAddress pos_data_address;
	VkDeviceAddress attrib_data_address;

	glm::vec3 chunk_base_camworld;
	float chunk_size_metres;
};

} // namespace

void LegacyRenderGraph::rebuild(RenderGraphBuilder &bld)
{
	m_device = &bld.device();

	// Frustum culling pass
	{
		// auto &renderer = client::vulkan::Backend::backend().terrainRenderer();
		m_res.terrain_combo_buffer = bld.makeBuffer("combo_buffer",
			{
				.size = 4096 //renderer.getComboBufferSize(),
			});

		// TODO: this buffer is not actually used; terrain renderer
		// has internal one that should be replaced by this.
		RenderGraphBuilder::ResourceUsage usage[] = {
			bld.makeBufferUsage(m_res.terrain_combo_buffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
				VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, true),
		};

		bld.makeComputePass<&LegacyRenderGraph::doFrustumCullingPass>("Frustum culling", usage);
	}

	// Main pass
	{
		m_res.depth_buffer = bld.make2DImage("scene_depth",
			{
				.format = DEPTH_BUFFER_FORMAT,
				.resolution = bld.outputImageExtent(),
			});

		m_output_format = bld.outputImageFormat();
		m_output_resolution = bld.outputImageExtent();

		auto rtv = bld.makeOutputRenderTarget(true, CLEAR_COLOR);
		auto dsv = bld.makeDepthStencilTargetClearDiscard(m_res.depth_buffer, CLEAR_DEPTH);

		RenderGraphBuilder::ResourceUsage usage[] = {
			bld.makeBufferUsage(m_res.terrain_combo_buffer,
				VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT /*| VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT*/,
				VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT /*| VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT*/),
		};

		bld.makeRenderPass<&LegacyRenderGraph::doMainPass>("Main pass", rtv, dsv, usage);
	}
}

void LegacyRenderGraph::beginExecution(RenderGraphExecution &exec)
{
	assert(m_world_state);
	assert(m_game_view);

	auto &backend = client::vulkan::Backend::backend();
	auto &fctx = exec.frameContext();

	VkDescriptorSet main_scene_dset = createMainSceneDset(fctx);
	VkDescriptorSet frustum_cull_dset = fctx.allocateDescriptorSet(
		backend.descriptorSetLayoutCollection().terrainFrustumCullLayout());

	auto &renderer = backend.terrainRenderer();
	renderer.onNewWorldState(*m_world_state);
	renderer.onFrameBegin(*m_game_view, main_scene_dset, frustum_cull_dset);
	renderer.prepareResources(fctx.commandBuffer());

	m_main_scene_dset = main_scene_dset;
}

void LegacyRenderGraph::endExecution(RenderGraphExecution &)
{
	// Just a precaution to segfault instead of using dangling ref
	m_world_state = nullptr;
	m_game_view = nullptr;
}

void LegacyRenderGraph::setGameState(const WorldState &state, const GameView &view)
{
	m_world_state = &state;
	m_game_view = &view;
}

void LegacyRenderGraph::doFrustumCullingPass(RenderGraphExecution &exec)
{
	auto &renderer = client::vulkan::Backend::backend().terrainRenderer();
	renderer.launchFrustumCull(exec.frameContext().commandBuffer());
}

void LegacyRenderGraph::doMainPass(RenderGraphExecution &exec)
{
	VkCommandBuffer cmd_buf = exec.frameContext().commandBuffer();
	auto &ddt = m_device->dt();

	const VkViewport viewport {
		.x = 0.0f,
		.y = 0.0f,
		.width = float(m_output_resolution.width),
		.height = float(m_output_resolution.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	ddt.vkCmdSetViewport(cmd_buf, 0, 1, &viewport);

	auto &legacy_backend = client::vulkan::Backend::backend();

	//auto &renderer = legacy_backend.terrainRenderer();
	//renderer.drawChunksInFrustum(cmd_buf);
	//renderer.drawDebugChunkBorders(cmd_buf);

	// Draw land impostors

	const glm::dvec3 viewpoint = m_game_view->cameraPosition();

	LandLoader *land_loader = legacy_backend.gfxSystem().landLoader();

	using DrawCmd = LandLoader::DrawCommand;
	using DrawList = LandLoader::DrawList;

	DrawList dlist;
#if 0
	land_loader->makeDrawList(glm::dvec3(0, 80, 0), dlist);
#else
	land_loader->makeDrawList(viewpoint, dlist);
#endif

	VkPipelineLayout chunk_mesh_pipeline_layout = legacy_backend.pipelineLayoutCollection().landChunkMeshLayout();

	if (!dlist.empty()) {
		// We will have to switch states when index buffers change.
		// Sort commands to aggregate (batch) them by buffer handle.
		// XXX: can also approximately order front-to-back while we're at it.
		std::sort(dlist.begin(), dlist.end(),
			[&](const DrawCmd &a, const DrawCmd &b) { return a.index_buffer < b.index_buffer; });

		ddt.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
			legacy_backend.pipelineCollection()[client::vulkan::PipelineCollection::LAND_CHUNK_MESH_PIPELINE]);

		ddt.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, chunk_mesh_pipeline_layout, 0, 1,
			&m_main_scene_dset, 0, nullptr);
	}

	for (auto range_begin = dlist.begin(); range_begin != dlist.end(); /*no-op*/) {
		const VkBuffer index_buffer = range_begin->index_buffer;
		auto range_end = range_begin;

		// Collect all draw commands with the same index buffer
		while (range_end != dlist.end() && range_end->index_buffer == index_buffer) {
			++range_end;
		}

		const uint32_t draws = uint32_t(std::distance(range_begin, range_end));

		VkDeviceSize indirect_size = sizeof(VkDrawIndexedIndirectCommand) * draws;
		auto indirect_upload = exec.frameContext().allocateConstantUpload(indirect_size);

		VkDeviceSize cmd_size = sizeof(PseudoSurfaceDrawCommand) * draws;
		auto cmd_upload = exec.frameContext().allocateConstantUpload(cmd_size);

		auto *indirect_cmd_array = reinterpret_cast<VkDrawIndexedIndirectCommand *>(
			indirect_upload.host_mapped_span.data());
		auto *draw_cmd_array = reinterpret_cast<PseudoSurfaceDrawCommand *>(cmd_upload.host_mapped_span.data());

		for (uint32_t i = 0; i < draws; i++) {
			const DrawCmd &dcmd = range_begin[i];

			// Calculate chunk world position in doubles, subtract from camera position in doubles,
			// and only then cast the difference (smaller value) to floats to pass it to GPU.
			glm::dvec3 chunk_base_world = glm::dvec3(dcmd.chunk_key.base()) * land::Consts::CHUNK_SIZE_METRES;

			indirect_cmd_array[i] = VkDrawIndexedIndirectCommand {
				.indexCount = dcmd.num_indices,
				.instanceCount = 1,
				.firstIndex = dcmd.first_index,
				.vertexOffset = 0,
				.firstInstance = 0,
			};

			draw_cmd_array[i] = PseudoSurfaceDrawCommand {
				.pos_data_address = dcmd.pos_data_address,
				.attrib_data_address = dcmd.attrib_data_address,
				.chunk_base_camworld = glm::vec3(chunk_base_world - viewpoint),
				.chunk_size_metres = float(land::Consts::CHUNK_SIZE_METRES) * float(dcmd.chunk_key.scaleMultiplier()),
			};
		}

		VkDescriptorBufferInfo cbuf_info {
			.buffer = cmd_upload.buffer,
			.offset = cmd_upload.offset,
			.range = cmd_size,
		};

		VkWriteDescriptorSet descriptor {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = VK_NULL_HANDLE,
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pImageInfo = nullptr,
			.pBufferInfo = &cbuf_info,
			.pTexelBufferView = nullptr,
		};

		ddt.vkCmdBindIndexBuffer(cmd_buf, index_buffer, 0, VK_INDEX_TYPE_UINT16);
		ddt.vkCmdPushDescriptorSetKHR(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, chunk_mesh_pipeline_layout, 1, 1,
			&descriptor);
		ddt.vkCmdDrawIndexedIndirect(cmd_buf, indirect_upload.buffer, indirect_upload.offset, draws,
			sizeof(VkDrawIndexedIndirectCommand));

		range_begin = range_end;
	}

	// Draw target block selector
	{
		glm::dvec3 modify_target_block_pos = glm::dvec3(m_game_view->modifyTargetBlockCoord())
			* land::Consts::BLOCK_SIZE_METRES;

		struct {
			glm::vec3 modify_target_camworld;
			float cube_size_world;
		} push_const;

		push_const.modify_target_camworld = glm::vec3(modify_target_block_pos - viewpoint);
		push_const.cube_size_world = float(land::Consts::BLOCK_SIZE_METRES);

		VkPipelineLayout layout = legacy_backend.pipelineLayoutCollection().landSelectorLayout();

		// TODO: don't upload index buffer again every frame
		constexpr uint16_t INDEX_BUFFER[36] = { // X+
			6, 3, 2, 3, 6, 7,
			// X-
			1, 4, 0, 1, 5, 4,
			// Y+
			7, 6, 4, 7, 4, 5,
			// Y-
			3, 1, 0, 0, 2, 3,
			// Z+
			5, 1, 3, 3, 7, 5,
			// Z-
			4, 2, 0, 2, 4, 6
		};

		auto index_buffer
			= legacy_backend.gfxSystem().transientBufferAllocator()->allocate(TransientBufferAllocator::TypeUpload,
				sizeof(INDEX_BUFFER), 4);

		memcpy(index_buffer.host_pointer, INDEX_BUFFER, sizeof(INDEX_BUFFER));
		ddt.vkCmdBindIndexBuffer(cmd_buf, index_buffer.buffer, index_buffer.buffer_offset, VK_INDEX_TYPE_UINT16);

		ddt.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
			legacy_backend.pipelineCollection()[client::vulkan::PipelineCollection::LAND_SELECTOR_PIPELINE]);
		ddt.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &m_main_scene_dset, 0,
			nullptr);
		ddt.vkCmdPushConstants(cmd_buf, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_const), &push_const);
		ddt.vkCmdDrawIndexed(cmd_buf, 36, 1, 0, 0, 0);
	}

	const land::Chunk::BlockId current_block = m_game_view->selectedBlockId();

	FontRenderer::TextItem texts[] = { {
		.text = land::TempBlockMeta::BLOCK_NAME[current_block],
		.origin_screen = glm::vec2(viewport.width * 0.5f - 75.0f, 50.0f),
		.color = land::TempBlockMeta::BLOCK_FIXED_COLOR[current_block],
	} };

	FontRenderer &font = *legacy_backend.gfxSystem().fontRenderer();
	font.drawUi(cmd_buf, texts, glm::vec2(1.0f / viewport.width, 1.0f / viewport.height));
}

VkDescriptorSet LegacyRenderGraph::createMainSceneDset(FrameContext &fctx)
{
	auto upload = fctx.allocateConstantUpload(sizeof(MainSceneUbo));

	auto *ubo_data = reinterpret_cast<MainSceneUbo *>(upload.host_mapped_span.data());

	ubo_data->translated_world_to_clip = m_game_view->translatedWorldToClip();
	ubo_data->world_position = m_game_view->cameraPosition();

	auto &dslc = client::vulkan::Backend::backend().descriptorSetLayoutCollection();
	VkDescriptorSet dset = fctx.allocateDescriptorSet(dslc.mainSceneLayout());

	const VkDescriptorBufferInfo buffer_info {
		.buffer = upload.buffer,
		.offset = upload.offset,
		.range = sizeof(MainSceneUbo),
	};

	const VkWriteDescriptorSet write {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.pNext = nullptr,
		.dstSet = dset,
		.dstBinding = 0,
		.dstArrayElement = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.pImageInfo = nullptr,
		.pBufferInfo = &buffer_info,
		.pTexelBufferView = nullptr,
	};

	m_device->vkUpdateDescriptorSets(1, &write, 0);
	return dset;
}

} // namespace voxen::gfx::vk
