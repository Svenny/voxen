#pragma once

#include <voxen/gfx/graph/render_graph_common.hpp>

namespace voxen::gfx::graph
{

struct ChunkDrawCommand {};

// This pass collects draw commands for all chunks
class CollectLandDrawsRenderPass : public IRenderPass {
public:
	void build(RenderGraphBuilder &bld) override {}

	void prepare(RenderGraphPreparation &prep) override {}

	void execute(RenderGraphExecution &exec) override {}

private:
	RenderGraphBuffer m_draw_commands_buffer;
	uint32_t m_generated_commands = 0;
};

class LandDrawsCullingRenderPass : public IRenderPass {
public:
	struct Args {
		uint32_t num_all_commands;
		RenderGraphBuffer all_commands_buffer;
		CameraInfo camera;
	};

	RenderPassInfo build(RenderGraphBuilder &bld) override
	{
		m_culled_commands_buffer = bld.declareDynamicBuffer();

		return {
			.name = "LandDrawsCullingRenderPass",
			.type = RenderPassType::Compute,
			.hint = RenderPassHint::Small,
		};
	}

	void prepare(RenderGraphPreparation &prep, Args args)
	{
		m_prepare_args = args;
		prep.use(m_prepare_args.all_commands_buffer, Stage::Compute, Usage::UAV | Usage::ReadOnly);

		prep.setDynamicBufferSize(m_culled_commands_buffer, sizeof(ChunkDrawCommand) * args.num_all_commands + 4);
		prep.produce(m_culled_commands_buffer, Stage::Compute, Usage::UAV | Usage::WriteOnly);
	}

	void execute(RenderGraphExecution &exec) override {}

private:
	Args m_prepare_args;
	RenderGraphBuffer m_culled_commands_buffer;
};

class DrawLandChunksRenderPass : public IRenderPass {
public:
	DrawLandChunksRenderPass(RenderGraphBuilder& bld, RenderPassDescription& desc, RenderGraphBuffer& draw_commands_buffer, RenderGraphImage& color_buffer, RenderGraphImage& depth_buffer)
	{

	}

	void prepare(RenderGraphPreparation& prep, uint32_t max_draw_commands)
	{
		prep.use(m_draw_commands_buffer, Stage::Vertex | Stage::Indirect, Usage::Indirect | Usage::ReadOnlyStorageBuffer);
		prep.setRenderTargets(m_color_buffer, m_depth_buffer);
		prep.useExecution(max_draw_commands);
	}

	void execute(RenderGraphExecution &exec, uint32_t max_draw_commands)
	{
		exec.cmd().drawIndexedIndirect(m_draw_commands_buffer, ..., max_draw_commands);
	}

private:
	RenderGraphBuffer &m_draw_commands_buffer;
	RenderGraphImage &m_color_buffer;
	RenderGraphImage &m_depth_buffer;
};

class DrawLandDebugBordersRenderPass : public IRenderPass {
public:
private:
};

class LandRenderPass : public IRenderPass {
public:
	LandRenderPass(RenderGraphBuilder &bld, RenderPassDescription &desc, RenderGraphImage &color_buffer,
		RenderGraphImage &depth_buffer)
	{
		desc.name = "LandRenderPass";
		desc.type = RenderPassType::SubGraph;

		m_collect_draws_pass = bld.addPass<CollectLandDrawsRenderPass>();
		m_draws_culling_pass = bld.addPass<LandDrawsCullingRenderPass>(m_collect_draws_pass->drawCommandsBuffer());

		m_draw_chunks_pass = bld.addPass<DrawLandChunksRenderPass>(m_draws_culling_pass->culledCommandsBuffer(),
			color_buffer, depth_buffer);
		m_draw_debug_borders_pass
			= bld.addPass<DrawLandDebugBordersRenderPass>(m_draws_culling_pass->culledCommandsBuffer(), color_buffer,
				depth_buffer);
	}

	void prepare(RenderGraphPreparation &prep)
	{
		Camera &camera = prep.gameState().player().camera();

		prep.usePass(m_collect_draws_pass);
		uint32_t num_draw_commands = m_collect_draws_pass->numDrawCommands();

		prep.usePass(m_draws_culling_pass, camera, num_draw_commands);
		prep.usePass(m_draw_chunks_pass, camera, num_draw_commands);
		prep.usePass(m_draw_debug_borders_pass, camera, num_draw_commands);
	}

private:
	RenderPassHandle<CollectLandDrawsRenderPass> m_collect_draws_pass;
	RenderPassHandle<LandDrawsCullingRenderPass> m_draws_culling_pass;
	RenderPassHandle<DrawLandChunksRenderPass> m_draw_chunks_pass;
	RenderPassHandle<DrawLandDebugBordersRenderPass> m_draw_debug_borders_pass;
};

} // namespace voxen::gfx::graph
