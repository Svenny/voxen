#pragma once

#include <voxen/common/gameview.hpp>
#include <voxen/common/world_state.hpp>
#include <voxen/gfx/vk/render_graph.hpp>
#include <voxen/gfx/vk/render_graph_resource.hpp>

namespace voxen::gfx::vk
{

class Device;
class FrameContext;

class LegacyRenderGraph final : public IRenderGraph {
public:
	constexpr static VkFormat DEPTH_BUFFER_FORMAT = VK_FORMAT_D32_SFLOAT;

	~LegacyRenderGraph() noexcept override = default;

	void rebuild(RenderGraphBuilder &bld) override;
	void beginExecution(RenderGraphExecution &exec) override;
	void endExecution(RenderGraphExecution &exec) override;

	void setGameState(const WorldState &state, const GameView &view);

	VkFormat currentOutputFormat() const noexcept { return m_output_format; }

private:
	void doFrustumCullingPass(RenderGraphExecution &exec);
	void doMainPass(RenderGraphExecution &exec);

	VkDescriptorSet createMainSceneDset(FrameContext &fctx);

	Device *m_device = nullptr;
	const WorldState *m_world_state = nullptr;
	const GameView *m_game_view = nullptr;

	VkDescriptorSet m_main_scene_dset = VK_NULL_HANDLE;

	VkFormat m_output_format = VK_FORMAT_UNDEFINED;
	VkExtent2D m_output_resolution = {};

	// Resource handles
	struct {
		RenderGraphBuffer terrain_combo_buffer;
		RenderGraphImage depth_buffer;
	} m_res;
};

} // namespace voxen::gfx::vk
