#pragma once

#include <voxen/common/gameview.hpp>
#include <voxen/common/world_state.hpp>
#include <voxen/gfx/gfx_fwd.hpp>
#include <voxen/gfx/vk/render_graph.hpp>
#include <voxen/gfx/vk/render_graph_resource.hpp>
#include <voxen/gfx/vk/vk_transient_buffer_allocator.hpp>

namespace voxen::gfx::vk
{

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
	struct LandPerIndexBufferData {
		VkBuffer index_buffer;
		uint32_t num_all_commands;
		// Stores array of `PseudoSurfacePreCullingDrawCommand` with all known chunks
		TransientBufferAllocator::Allocation pre_culling_commands;
		// Stores array of `PseudoSurfaceDrawCommand` that passed frustum culling.
		// The first element is uint32 counting the number of valid entries.
		TransientBufferAllocator::Allocation draw_commands;
		// Stores array of `VkDrawIndexedIndirectCommand` that passed frustum culling
		TransientBufferAllocator::Allocation indirect_commands;
	};

	void doFrustumCullingPass(RenderGraphExecution &exec);
	void doMainPass(RenderGraphExecution &exec);

	VkDescriptorSet createMainSceneDset(FrameContext &fctx);

	GfxSystem *m_gfx = nullptr;
	const WorldState *m_world_state = nullptr;
	const GameView *m_game_view = nullptr;

	VkDescriptorSet m_main_scene_dset = VK_NULL_HANDLE;
	std::vector<LandPerIndexBufferData> m_land_per_index_buffer_data;

	VkFormat m_output_format = VK_FORMAT_UNDEFINED;
	VkExtent2D m_output_resolution = {};

	// Resource handles
	struct {
		// TODO: we use dynamic buffer count for chunk draw commands,
		// this buffer serves only as a synchronization point
		RenderGraphBuffer dummy_sync_buffer;
		RenderGraphImage depth_buffer;
	} m_res;
};

} // namespace voxen::gfx::vk
