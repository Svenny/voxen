#include <voxen/gfx/vk/standard_render_graph.hpp>

#include <voxen/gfx/vk/frame_context.hpp>
#include <voxen/gfx/vk/render_graph_builder.hpp>
#include <voxen/gfx/vk/render_graph_execution.hpp>

#include <fmt/format.h>

namespace voxen::gfx::vk
{

namespace
{

constexpr static uint32_t SUN_CSM_CASCADES = 4;
constexpr VkFormat SUN_CSM_DEPTH_FORMAT = VK_FORMAT_D16_UNORM;
constexpr VkExtent2D SUN_CSM_RESOLUTION = { 1024, 1024 };
constexpr VkClearDepthStencilValue SUN_CSM_DEPTH_CLEAR_VALUE = { .depth = 0.0f, .stencil = 0 };

constexpr VkFormat MAIN_PASS_COLOR_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;
constexpr VkFormat MAIN_PASS_DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;
constexpr VkFormat MAIN_PASS_MV_FORMAT = VK_FORMAT_R16G16_UNORM;
constexpr VkExtent2D MAIN_PASS_RESOLUTION = { 1600, 900 };
constexpr VkClearDepthStencilValue MAIN_PASS_DEPTH_CLEAR_VALUE = { .depth = 0.0f, .stencil = 0 };

constexpr VkFormat LINEAR_DEPTH_FORMAT = VK_FORMAT_R32_SFLOAT;
constexpr VkFormat EXPOSURE_FORMAT = VK_FORMAT_R16_SFLOAT;
constexpr int32_t EXPOSURE_MIP_LEVEL = StandardRenderGraph::LINEAR_DEPTH_MIPS;

constexpr VkFormat AO_FACTOR_FORMAT = VK_FORMAT_R8_UNORM;

constexpr VkExtent2D FINAL_RESOLUTION = { 1920, 1080 };

VkExtent2D mip(VkExtent2D res, uint32_t mip)
{
	return { std::max(1u, res.width >> mip), std::max(1u, res.height >> mip) };
}

} // namespace

void StandardRenderGraph::rebuild(RenderGraphBuilder &bld)
{
	m_gfx = &bld.gfxSystem();

	// Frustum culling pass
	{
		m_res.main_pass_draw_cmds = bld.makeBuffer("main_pass_draw_cmds", { .dynamic_size = true });
		m_res.sun_pass_draw_cmds = bld.makeBuffer("sun_pass_draw_cmds", { .dynamic_size = true });

		RenderGraphBuilder::ResourceUsage usage[] = {
			bld.makeBufferUsage(m_res.main_pass_draw_cmds, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
				VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, true),
			bld.makeBufferUsage(m_res.sun_pass_draw_cmds, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
				VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, true),
		};

		bld.makeComputePass<&StandardRenderGraph::doFrustumCullingPass>("Frustum culling", usage);
	}

	// Sun CSM pass
	{
		m_res.sun_csm = bld.make2DImage("sun_csm",
			{
				.format = SUN_CSM_DEPTH_FORMAT,
				.resolution = SUN_CSM_RESOLUTION,
				.layers = SUN_CSM_CASCADES,
			});

		auto ds_target = bld.makeDepthStencilTargetClearStore(m_res.sun_csm, SUN_CSM_DEPTH_CLEAR_VALUE);

		RenderGraphBuilder::ResourceUsage usage[] = {
			bld.makeBufferUsage(m_res.sun_pass_draw_cmds, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
				VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT),
		};

		bld.makeDepthRenderPass<&StandardRenderGraph::doSunCsmPass>("Sun CSM", ds_target, usage);
	}

	m_views.sun_csm_srv = bld.makeBasicImageView("sun_csm/srv", m_res.sun_csm);

	// Main pass
	{
		m_res.main_color = bld.make2DImage("scene_color",
			{
				.format = MAIN_PASS_COLOR_FORMAT,
				.resolution = MAIN_PASS_RESOLUTION,
			});

		m_res.main_mv = bld.make2DImage("scene_mv",
			{
				.format = MAIN_PASS_MV_FORMAT,
				.resolution = MAIN_PASS_RESOLUTION,
			});

		RenderGraphBuilder::RenderTarget targets[] = {
			bld.makeRenderTargetDiscardStore(m_res.main_color),
			bld.makeRenderTargetDiscardStore(m_res.main_mv),
		};

		m_res.main_depth = bld.make2DImage("scene_depth",
			{
				.format = MAIN_PASS_DEPTH_FORMAT,
				.resolution = MAIN_PASS_RESOLUTION,
			});

		auto ds_target = bld.makeDepthStencilTargetClearStore(m_res.main_depth, MAIN_PASS_DEPTH_CLEAR_VALUE);

		RenderGraphBuilder::ResourceUsage usage[] = {
			bld.makeBufferUsage(m_res.main_pass_draw_cmds, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
				VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT),
			bld.makeSrvUsage(m_views.sun_csm_srv, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT),
		};

		bld.makeRenderPass<&StandardRenderGraph::doMainPass>("Main pass", targets, ds_target, usage);
	}

	m_views.main_color_srv = bld.makeBasicImageView("scene_color/srv", m_res.main_color);
	m_views.main_mv_srv = bld.makeBasicImageView("scene_mv/srv", m_res.main_mv);
	m_views.main_depth_srv = bld.makeBasicImageView("scene_depth/srv", m_res.main_depth);

	// Downsampling pass
	{
		m_res.linear_depth_mips = bld.make2DImage("linear_depth",
			{
				.format = LINEAR_DEPTH_FORMAT,
				.resolution = mip(MAIN_PASS_RESOLUTION, 1),
				.mips = LINEAR_DEPTH_MIPS,
			});

		std::tie(m_res.exposure, m_res.prev_exposure) = bld.makeDoubleBuffered2DImage("exposure",
			{
				.format = EXPOSURE_FORMAT,
				.resolution = mip(MAIN_PASS_RESOLUTION, EXPOSURE_MIP_LEVEL),
			});

		m_views.exposure_uav = bld.makeBasicImageView("exposure/uav", m_res.exposure);
		m_views.prev_exposure_srv = bld.makeBasicImageView("prev_exposure/srv", m_res.prev_exposure);

		RenderGraphBuilder::ResourceUsage usage[4 + LINEAR_DEPTH_MIPS] = {
			bld.makeSrvUsage(m_views.main_color_srv, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT),
			bld.makeSrvUsage(m_views.main_depth_srv, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT),
			bld.makeSrvUsage(m_views.prev_exposure_srv, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT),
			bld.makeImageViewUsage(m_views.exposure_uav, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
				VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, true),
		};

		for (uint32_t i = 0; i < LINEAR_DEPTH_MIPS; i++) {
			m_views.linear_depth_uav[i] = bld.makeSingleMipImageView(fmt::format("linear_depth/uav_mip{}", i),
				m_res.linear_depth_mips, i);

			usage[4 + i] = bld.makeImageViewUsage(m_views.linear_depth_uav[i], VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
				VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, true);
		}

		bld.makeComputePass<&StandardRenderGraph::doDownsamplingPass>("Linear depth + exposure", usage);
	}

	m_views.linear_depth_mips_srv = bld.makeBasicImageView("linear_depth/srv", m_res.linear_depth_mips);
	m_views.exposure_srv = bld.makeBasicImageView("exposure/srv", m_res.exposure);

	// SSAO pass
	{
		m_res.ao_factor = bld.make2DImage("ao_factor",
			{
				.format = AO_FACTOR_FORMAT,
				.resolution = mip(MAIN_PASS_RESOLUTION, 1),
			});

		m_views.ao_factor_uav = bld.makeBasicImageView("ao_factor/uav", m_res.ao_factor);

		RenderGraphBuilder::ResourceUsage usage[] = {
			bld.makeSrvUsage(m_views.linear_depth_mips_srv, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT),
			bld.makeImageViewUsage(m_views.ao_factor_uav, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
				VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, true),
		};

		bld.makeComputePass<&StandardRenderGraph::doSsaoPass>("SSAO", usage);
	}

	m_views.ao_factor_srv = bld.makeBasicImageView("ao_factor/srv", m_res.ao_factor);

	// TAA+upscale pass
	{
		std::tie(m_res.upscaled_color, m_res.prev_upscaled_color) = bld.makeDoubleBuffered2DImage("upscaled_color",
			{
				.format = MAIN_PASS_COLOR_FORMAT,
				.resolution = FINAL_RESOLUTION,
			});

		m_views.upscaled_color_uav = bld.makeBasicImageView("upscaled_color/uav", m_res.upscaled_color);
		m_views.prev_upscaled_color_srv = bld.makeBasicImageView("prev_upscaled_color/srv", m_res.prev_upscaled_color);

		RenderGraphBuilder::ResourceUsage usage[] = {
			bld.makeSrvUsage(m_views.main_color_srv, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT),
			bld.makeSrvUsage(m_views.main_mv_srv, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT),
			bld.makeSrvUsage(m_views.prev_upscaled_color_srv, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT),
			bld.makeSrvUsage(m_views.ao_factor_srv, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT),
			bld.makeImageViewUsage(m_views.upscaled_color_uav, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
				VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, true),
		};

		bld.makeComputePass<&StandardRenderGraph::doAaUpscalePass>("TAA+Upscale", usage);
	}

	m_views.upscaled_color_srv = bld.makeBasicImageView("upscaled_color/srv", m_res.upscaled_color);

	// Final pass
	{
		RenderGraphBuilder::RenderTarget targets[] = {
			bld.makeOutputRenderTarget(false),
		};

		RenderGraphBuilder::ResourceUsage usage[] = {
			bld.makeSrvUsage(m_views.upscaled_color_srv, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT),
			bld.makeSrvUsage(m_views.exposure_srv, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT),
		};

		bld.makeRenderPass<&StandardRenderGraph::doFinalPass>("Final pass", targets, {}, usage);
	}
}

void StandardRenderGraph::beginExecution(RenderGraphExecution &exec)
{
	VkDeviceSize cmds = sizeof(VkDrawIndexedIndirectCommand) * getNumDrawCommands();

	exec.setDynamicBufferSize(m_res.main_pass_draw_cmds, cmds);
	exec.setDynamicBufferSize(m_res.sun_pass_draw_cmds, SUN_CSM_CASCADES * cmds);
}

VkDeviceSize StandardRenderGraph::getNumDrawCommands()
{
	// TODO: get from number of drawn objects
	return 1000;
}

void StandardRenderGraph::doFrustumCullingPass(RenderGraphExecution & /*exec*/) {}
void StandardRenderGraph::doSunCsmPass(RenderGraphExecution & /*exec*/) {}
void StandardRenderGraph::doMainPass(RenderGraphExecution & /*exec*/) {}
void StandardRenderGraph::doDownsamplingPass(RenderGraphExecution & /*exec*/) {}
void StandardRenderGraph::doSsaoPass(RenderGraphExecution & /*exec*/) {}
void StandardRenderGraph::doAaUpscalePass(RenderGraphExecution & /*exec*/) {}
void StandardRenderGraph::doFinalPass(RenderGraphExecution & /*exec*/) {}

} // namespace voxen::gfx::vk
