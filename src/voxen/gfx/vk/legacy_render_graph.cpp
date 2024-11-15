#include <voxen/gfx/vk/legacy_render_graph.hpp>

#include <voxen/client/vulkan/algo/terrain_renderer.hpp>
#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/descriptor_set_layout.hpp>
#include <voxen/gfx/vk/frame_context.hpp>
#include <voxen/gfx/vk/render_graph_builder.hpp>
#include <voxen/gfx/vk/render_graph_execution.hpp>
#include <voxen/gfx/vk/vk_device.hpp>

namespace voxen::gfx::vk
{

namespace
{

struct MainSceneUbo {
	glm::mat4 translated_world_to_clip;
	glm::vec3 world_position;
	float _pad0;
};

constexpr VkClearColorValue CLEAR_COLOR = { { 0.0f, 0.0f, 0.0f, 1.0f } };
constexpr VkClearDepthStencilValue CLEAR_DEPTH = { 0.0f, 0 };

} // namespace

void LegacyRenderGraph::rebuild(RenderGraphBuilder &bld)
{
	m_device = &bld.device();

	// Frustum culling pass
	{
		// auto &renderer = client::vulkan::Backend::backend().terrainRenderer();
		m_res.terrain_combo_buffer = bld.makeBuffer("combo_buffer",
			{
				.size = 4096//renderer.getComboBufferSize(),
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

	const VkViewport viewport {
		.x = 0.0f,
		.y = 0.0f,
		.width = float(m_output_resolution.width),
		.height = float(m_output_resolution.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	m_device->dt().vkCmdSetViewport(cmd_buf, 0, 1, &viewport);

	auto &renderer = client::vulkan::Backend::backend().terrainRenderer();
	renderer.drawChunksInFrustum(cmd_buf);
	renderer.drawDebugChunkBorders(cmd_buf);
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
