#pragma once

#include <voxen/gfx/gfx_fwd.hpp>
#include <voxen/gfx/vk/render_graph.hpp>
#include <voxen/gfx/vk/render_graph_resource.hpp>

#include <array>

namespace voxen::gfx::vk
{

class VOXEN_API StandardRenderGraph : public IRenderGraph {
public:
	constexpr static uint32_t LINEAR_DEPTH_MIPS = 4;

	~StandardRenderGraph() noexcept override = default;

	void rebuild(RenderGraphBuilder &bld) override;
	void beginExecution(RenderGraphExecution &exec) override;

private:
	VkDeviceSize getNumDrawCommands();

	void doFrustumCullingPass(RenderGraphExecution &exec);
	void doSunCsmPass(RenderGraphExecution &exec);
	void doMainPass(RenderGraphExecution &exec);
	void doDownsamplingPass(RenderGraphExecution &exec);
	void doSsaoPass(RenderGraphExecution &exec);
	void doAaUpscalePass(RenderGraphExecution &exec);
	void doFinalPass(RenderGraphExecution &exec);

	GfxSystem *m_gfx = nullptr;

	// Resource handles
	struct {
		RenderGraphBuffer main_pass_draw_cmds;
		RenderGraphBuffer sun_pass_draw_cmds;

		RenderGraphImage sun_csm;

		RenderGraphImage main_color;
		RenderGraphImage main_mv;
		RenderGraphImage main_depth;

		RenderGraphImage linear_depth_mips;
		RenderGraphImage exposure;
		RenderGraphImage prev_exposure;

		RenderGraphImage ao_factor;

		RenderGraphImage upscaled_color;
		RenderGraphImage prev_upscaled_color;
	} m_res;

	// Resource view handles
	struct {
		RenderGraphImageView sun_csm_srv;

		RenderGraphImageView main_color_srv;
		RenderGraphImageView main_mv_srv;
		RenderGraphImageView main_depth_srv;

		std::array<RenderGraphImageView, LINEAR_DEPTH_MIPS> linear_depth_uav;
		RenderGraphImageView exposure_uav;

		RenderGraphImageView linear_depth_mips_srv;
		RenderGraphImageView exposure_srv;
		RenderGraphImageView prev_exposure_srv;

		RenderGraphImageView ao_factor_uav;
		RenderGraphImageView ao_factor_srv;

		RenderGraphImageView upscaled_color_uav;
		RenderGraphImageView upscaled_color_srv;
		RenderGraphImageView prev_upscaled_color_srv;
	} m_views;
};

} // namespace voxen::gfx::vk
