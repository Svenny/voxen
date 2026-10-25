module frustum_cull;

import land.mesh_layouts;
import util.aabb;
import util.vk_structs;

#include "shared/main_scene.h"

struct PreCullingDrawCommand {
	// Copied to `DrawCommand` as is, see there why these are not pointers
	uint64_t pos_data_address;
	uint64_t attrib_data_address;

	float3 chunk_base_camworld;
	float chunk_size_metres;

	uint first_index;
	uint num_indices;
};

[[vk::binding(0, 0)]] ConstantBuffer<MainSceneUbo> g_main_scene;

[[vk::binding(0, 1)]] StructuredBuffer<PreCullingDrawCommand> g_pre_culling_cmds;
// Output draw command list (count + commands), see `drawCmdListItemOffset`
[[vk::binding(1, 1)]] RWByteAddressBuffer g_draw_cmds;
[[vk::binding(2, 1)]] RWStructuredBuffer<VkDrawIndexedIndirectCommand> g_indirect_cmds;

[shader("compute")]
[numthreads(64, 1, 1)]
[MaximallyReconverges]
// Explicitly required capabilities, otherwise slangc warns about implicit profile upgrade
[require(spirv, spvGroupNonUniformBallot, spvGroupNonUniform)]
void computeMain(uint3 global_id: SV_DispatchThreadID)
{
	const uint cmd_index = global_id.x;
	bool in_frustum = false;
	PreCullingDrawCommand pre_culling_cmd = {};

	if (cmd_index < g_pre_culling_cmds.getCount()) {
		pre_culling_cmd = g_pre_culling_cmds[cmd_index];

		Aabb aabb;
		aabb.min = pre_culling_cmd.chunk_base_camworld;
		aabb.max = aabb.min + pre_culling_cmd.chunk_size_metres;

		in_frustum = isAabbInFrustum(aabb, g_main_scene.translated_world_to_clip);
	}

	// Count of items in this subgroup that passed the culling
	uint ballot_bit_count = WaveActiveCountBits(in_frustum);

	if (ballot_bit_count == 0) {
		// No item in this subgroup passed the culling
		return;
	}

	// The first active thread performs the atomic to get the first ouptut slot,
	// then every thread calculates its own slot with ballot bit count scan.
	uint first_out_draw_index = 0;

	if (WaveIsFirstLane()) {
		g_draw_cmds.InterlockedAdd(DRAW_CMD_LIST_COUNT_OFFSET, ballot_bit_count, first_out_draw_index);
	}

	// Per-thread output item index, garbage for threads with `in_frustum == false`
	uint out_draw_index = WaveReadLaneFirst(first_out_draw_index) + WavePrefixCountBits(in_frustum);

	if (in_frustum) {
		DrawCommand draw_cmd;
		draw_cmd.pos_data_address = pre_culling_cmd.pos_data_address;
		draw_cmd.attrib_data_address = pre_culling_cmd.attrib_data_address;
		draw_cmd.chunk_base_camworld = pre_culling_cmd.chunk_base_camworld;
		draw_cmd.chunk_size_metres = pre_culling_cmd.chunk_size_metres;
		g_draw_cmds.Store<DrawCommand>(drawCmdListItemOffset(out_draw_index), draw_cmd);

		VkDrawIndexedIndirectCommand indirect_cmd;
		indirect_cmd.indexCount = pre_culling_cmd.num_indices;
		indirect_cmd.instanceCount = 1;
		indirect_cmd.firstIndex = pre_culling_cmd.first_index;
		indirect_cmd.vertexOffset = 0;
		indirect_cmd.firstInstance = 1;
		g_indirect_cmds[out_draw_index] = indirect_cmd;
	}
}
