#include <voxen/gfx/vk/legacy_render_graph.hpp>

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
struct PseudoSurfacePreCullingDrawCommand {
	VkDeviceAddress pos_data_address;
	VkDeviceAddress attrib_data_address;

	glm::vec3 chunk_base_camworld;
	float chunk_size_metres;

	uint32_t first_index;
	uint32_t num_indices;
};

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
	m_gfx = &bld.gfxSystem();

	// Frustum culling pass
	{
		m_res.dummy_sync_buffer = bld.makeBuffer("dummy_sync_buffer", { .size = 16 });

		RenderGraphBuilder::ResourceUsage usage[] = {
			bld.makeBufferUsage(m_res.dummy_sync_buffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
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
			bld.makeBufferUsage(m_res.dummy_sync_buffer, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
				VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT),
		};

		bld.makeRenderPass<&LegacyRenderGraph::doMainPass>("Main pass", rtv, dsv, usage);
	}
}

void LegacyRenderGraph::beginExecution(RenderGraphExecution &exec)
{
	assert(m_world_state);
	assert(m_game_view);

	m_main_scene_dset = createMainSceneDset(exec.frameContext());

	// Upload pre-culling land chunk draw commands

	const glm::dvec3 viewpoint = m_game_view->cameraPosition();

	LandLoader *land_loader = m_gfx->landLoader();
	TransientBufferAllocator *tsballoc = m_gfx->transientBufferAllocator();

	using DrawCmd = LandLoader::DrawCommand;
	using DrawList = LandLoader::DrawList;

	DrawList dlist;
#if 0
	land_loader->makeDrawList(glm::dvec3(0, 80, 0), dlist);
#else
	land_loader->makeDrawList(viewpoint, dlist);
#endif

	m_land_per_index_buffer_data.clear();

	if (dlist.empty()) {
		return;
	}

	// We will have to switch states when index buffers change.
	// Sort commands to aggregate (batch) them by buffer handle.
	// XXX: can also approximately order front-to-back while we're at it.
	std::sort(dlist.begin(), dlist.end(),
		[&](const DrawCmd &a, const DrawCmd &b) { return a.index_buffer < b.index_buffer; });

	for (auto range_begin = dlist.begin(); range_begin != dlist.end(); /*no-op*/) {
		const VkBuffer index_buffer = range_begin->index_buffer;
		auto range_end = range_begin;

		// Collect all draw commands with the same index buffer
		while (range_end != dlist.end() && range_end->index_buffer == index_buffer) {
			++range_end;
		}

		const uint32_t draws = uint32_t(std::distance(range_begin, range_end));

		// TODO: source it from `minStorageBufferOffsetAlignment` etc.
		const VkDeviceSize align = 64;
		const auto upload_type = TransientBufferAllocator::TypeUpload;
		const auto scratch_type = TransientBufferAllocator::TypeScratch;

		const VkDeviceSize pre_culling_cmds_size = sizeof(PseudoSurfacePreCullingDrawCommand) * draws;
		// 4 bytes for uint32 draws counter, 12 more to align to vec4 (16 bytes).
		// That alignment is not really needed (we have scalar buffer layout) but why not.
		const VkDeviceSize draw_cmds_size = 16 + sizeof(PseudoSurfaceDrawCommand) * draws;
		const VkDeviceSize indirect_cmds_size = sizeof(VkDrawIndexedIndirectCommand) * draws;

		LandPerIndexBufferData &per_ib_data = m_land_per_index_buffer_data.emplace_back();
		per_ib_data.index_buffer = range_begin->index_buffer;
		per_ib_data.num_all_commands = draws;
		per_ib_data.pre_culling_commands = tsballoc->allocate(upload_type, pre_culling_cmds_size, align);
		per_ib_data.draw_commands = tsballoc->allocate(scratch_type, draw_cmds_size, align);
		per_ib_data.indirect_commands = tsballoc->allocate(scratch_type, indirect_cmds_size, align);

		// Fill pre-culling commands
		auto *pre_culling_cmds = reinterpret_cast<PseudoSurfacePreCullingDrawCommand *>(
			per_ib_data.pre_culling_commands.host_pointer);

		for (uint32_t i = 0; i < draws; i++) {
			const DrawCmd &in_cmd = range_begin[i];
			PseudoSurfacePreCullingDrawCommand &out_cmd = pre_culling_cmds[i];

			// Calculate chunk world position in doubles, subtract from camera position in doubles,
			// and only then cast the difference (smaller value) to floats to pass it to GPU.
			glm::dvec3 chunk_base_world = glm::dvec3(in_cmd.chunk_key.base()) * land::Consts::CHUNK_SIZE_METRES;

			out_cmd.pos_data_address = in_cmd.pos_data_address;
			out_cmd.attrib_data_address = in_cmd.attrib_data_address;
			out_cmd.chunk_base_camworld = glm::vec3(chunk_base_world - viewpoint);
			out_cmd.chunk_size_metres = float(land::Consts::CHUNK_SIZE_METRES) * float(in_cmd.chunk_key.scaleMultiplier());
			out_cmd.first_index = in_cmd.first_index;
			out_cmd.num_indices = in_cmd.num_indices;
		}

		range_begin = range_end;
	}
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
	if (m_land_per_index_buffer_data.empty()) {
		return;
	}

	VkCommandBuffer cmd_buf = exec.frameContext().commandBuffer();
	auto &ddt = m_gfx->device()->dt();

	auto &legacy_backend = client::vulkan::Backend::backend();
	auto &legacy_pipeline_collection = legacy_backend.pipelineCollection();
	auto &legacy_layout_collection = legacy_backend.pipelineLayoutCollection();

	VkPipelineLayout layout = legacy_layout_collection.landFrustumCullLayout();

	ddt.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE,
		legacy_pipeline_collection[client::vulkan::PipelineCollection::LAND_FRUSTUM_CULL_PIPELINE]);
	ddt.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &m_main_scene_dset, 0, nullptr);

	for (LandPerIndexBufferData &per_ib_data : m_land_per_index_buffer_data) {
		// Clear draw counter to zero
		ddt.vkCmdFillBuffer(cmd_buf, per_ib_data.draw_commands.buffer, per_ib_data.draw_commands.buffer_offset,
			sizeof(uint32_t), 0);

		VkDescriptorBufferInfo buffer_descriptors[3] = {
			{
				// 0 - input buffer (pre-culled cmds)
				.buffer = per_ib_data.pre_culling_commands.buffer,
				.offset = per_ib_data.pre_culling_commands.buffer_offset,
				.range = sizeof(PseudoSurfacePreCullingDrawCommand) * per_ib_data.num_all_commands,
			},
			{
				// 1 - output buffer (draw cmds + counter)
				.buffer = per_ib_data.draw_commands.buffer,
				.offset = per_ib_data.draw_commands.buffer_offset,
				.range = sizeof(PseudoSurfaceDrawCommand) * per_ib_data.num_all_commands,
			},
			{
				// 2 - output buffer (indirect cmds)
				.buffer = per_ib_data.indirect_commands.buffer,
				.offset = per_ib_data.indirect_commands.buffer_offset,
				.range = sizeof(VkDrawIndexedIndirectCommand) * per_ib_data.num_all_commands,
			},
		};

		VkWriteDescriptorSet descriptor_write {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = VK_NULL_HANDLE,
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = std::size(buffer_descriptors),
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pImageInfo = nullptr,
			.pBufferInfo = buffer_descriptors,
			.pTexelBufferView = nullptr,
		};

		ddt.vkCmdPushDescriptorSetKHR(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 1, 1, &descriptor_write);
		ddt.vkCmdDispatch(cmd_buf, (per_ib_data.num_all_commands + 63) / 64, 1, 1);
	}
}

void LegacyRenderGraph::doMainPass(RenderGraphExecution &exec)
{
	VkCommandBuffer cmd_buf = exec.frameContext().commandBuffer();
	auto &ddt = m_gfx->device()->dt();

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
	auto &legacy_pipeline_collection = legacy_backend.pipelineCollection();
	auto &legacy_layout_collection = legacy_backend.pipelineLayoutCollection();

	// Draw land chunk meshes that have passed frustum culling
	if (!m_land_per_index_buffer_data.empty()) {
		VkPipelineLayout chunk_mesh_pipeline_layout = legacy_layout_collection.landChunkMeshLayout();

		ddt.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
			legacy_pipeline_collection[client::vulkan::PipelineCollection::LAND_CHUNK_MESH_PIPELINE]);

		ddt.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, chunk_mesh_pipeline_layout, 0, 1,
			&m_main_scene_dset, 0, nullptr);

		for (LandPerIndexBufferData &per_ib_data : m_land_per_index_buffer_data) {
			VkDescriptorBufferInfo buffer_descriptor {
				.buffer = per_ib_data.draw_commands.buffer,
				.offset = per_ib_data.draw_commands.buffer_offset,
				.range = sizeof(PseudoSurfaceDrawCommand) * per_ib_data.num_all_commands,
			};

			VkWriteDescriptorSet descriptor_write {
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = VK_NULL_HANDLE,
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &buffer_descriptor,
				.pTexelBufferView = nullptr,
			};

			ddt.vkCmdBindIndexBuffer(cmd_buf, per_ib_data.index_buffer, 0, VK_INDEX_TYPE_UINT16);
			ddt.vkCmdPushDescriptorSetKHR(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, chunk_mesh_pipeline_layout, 1, 1,
				&descriptor_write);
			ddt.vkCmdDrawIndexedIndirectCount(cmd_buf, per_ib_data.indirect_commands.buffer,
				per_ib_data.indirect_commands.buffer_offset, per_ib_data.draw_commands.buffer,
				per_ib_data.draw_commands.buffer_offset, per_ib_data.num_all_commands,
				sizeof(VkDrawIndexedIndirectCommand));
		}
	}

	// Draw debug chunk bounds
	if (!m_land_per_index_buffer_data.empty()) {
		VkPipelineLayout pipeline_layout = legacy_layout_collection.landChunkMeshLayout();

		// TODO: don't upload index buffer again every frame
		constexpr uint16_t INDEX_BUFFER[] = { 0, 1, 1, 5, 4, 5, 0, 4, 2, 3, 3, 7, 6, 7, 2, 6, 0, 2, 1, 3, 4, 6, 5, 7 };

		auto index_buffer = m_gfx->transientBufferAllocator()->allocate(TransientBufferAllocator::TypeUpload,
			sizeof(INDEX_BUFFER), 4);

		memcpy(index_buffer.host_pointer, INDEX_BUFFER, sizeof(INDEX_BUFFER));
		ddt.vkCmdBindIndexBuffer(cmd_buf, index_buffer.buffer, index_buffer.buffer_offset, VK_INDEX_TYPE_UINT16);

		ddt.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
			legacy_pipeline_collection[client::vulkan::PipelineCollection::LAND_DEBUG_CHUNK_BOUNDS_PIPELINE]);

		ddt.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &m_main_scene_dset,
			0, nullptr);

		for (LandPerIndexBufferData &per_ib_data : m_land_per_index_buffer_data) {
			VkDescriptorBufferInfo buffer_descriptor {
				.buffer = per_ib_data.draw_commands.buffer,
				.offset = per_ib_data.draw_commands.buffer_offset,
				.range = sizeof(PseudoSurfaceDrawCommand) * per_ib_data.num_all_commands,
			};

			VkWriteDescriptorSet descriptor_write {
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = VK_NULL_HANDLE,
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &buffer_descriptor,
				.pTexelBufferView = nullptr,
			};

			ddt.vkCmdPushDescriptorSetKHR(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 1, 1,
				&descriptor_write);
			// XXX: not the most efficient way to draw: dispatch the maximal
			// possible number of instances, discard the unneeded ones in VS.
			// The proper number (commands passed culling) is only available on GPU.
			// Might do an indirect draw instead but that would require one more indirect buffer.
			ddt.vkCmdDrawIndexed(cmd_buf, 24, per_ib_data.num_all_commands, 0, 0, 0);
		}
	}

	// Draw target block selector
	{
		glm::dvec3 modify_target_block_pos = glm::dvec3(m_game_view->modifyTargetBlockCoord())
			* land::Consts::BLOCK_SIZE_METRES;

		struct {
			glm::vec3 modify_target_camworld;
			float cube_size_world;
		} push_const;

		const glm::dvec3 viewpoint = m_game_view->cameraPosition();
		push_const.modify_target_camworld = glm::vec3(modify_target_block_pos - viewpoint);
		push_const.cube_size_world = float(land::Consts::BLOCK_SIZE_METRES);

		VkPipelineLayout layout = legacy_layout_collection.landSelectorLayout();

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

		auto index_buffer = m_gfx->transientBufferAllocator()->allocate(TransientBufferAllocator::TypeUpload,
			sizeof(INDEX_BUFFER), 4);

		memcpy(index_buffer.host_pointer, INDEX_BUFFER, sizeof(INDEX_BUFFER));
		ddt.vkCmdBindIndexBuffer(cmd_buf, index_buffer.buffer, index_buffer.buffer_offset, VK_INDEX_TYPE_UINT16);

		ddt.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
			legacy_pipeline_collection[client::vulkan::PipelineCollection::LAND_SELECTOR_PIPELINE]);
		ddt.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &m_main_scene_dset, 0,
			nullptr);
		ddt.vkCmdPushConstants(cmd_buf, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_const), &push_const);
		ddt.vkCmdDrawIndexed(cmd_buf, 36, 1, 0, 0, 0);
	}

	// Draw selected block name text
	{
		const land::Chunk::BlockId current_block = m_game_view->selectedBlockId();

		FontRenderer::TextItem texts[] = { {
			.text = land::TempBlockMeta::BLOCK_NAME[current_block],
			.origin_screen = glm::vec2(viewport.width * 0.5f - 75.0f, 50.0f),
			.color = land::TempBlockMeta::BLOCK_FIXED_COLOR[current_block],
		} };

		FontRenderer &font = *m_gfx->fontRenderer();
		font.drawUi(cmd_buf, texts, glm::vec2(1.0f / viewport.width, 1.0f / viewport.height));
	}
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

	m_gfx->device()->vkUpdateDescriptorSets(1, &write, 0);
	return dset;
}

} // namespace voxen::gfx::vk
