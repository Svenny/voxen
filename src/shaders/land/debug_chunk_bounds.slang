module debug_chunk_bounds;

import land.mesh_layouts;

#include "shared/main_scene.h"

[[vk::binding(0, 0)]] ConstantBuffer<MainSceneUbo> g_main_scene;
// Draw command list, indexed by instance ID
[[vk::binding(0, 1)]] ByteAddressBuffer g_cmd_list;

struct Varyings {
	[[vk::location(0)]] float3 color;
};

float3 calcColor(float scale)
{
	float green = min(log2(scale), 8.0) / 8.0;
	return float3(1.0 - green, green, 0.0);
}

[shader("vertex")]
Varyings vertexMain(uint vertex_index: SV_VulkanVertexID, uint instance_index: SV_VulkanInstanceID,
	out float4 position: SV_Position)
{
	Varyings output = {};

	if (instance_index >= g_cmd_list.Load<uint>(DRAW_CMD_LIST_COUNT_OFFSET)) {
		position = float4(0, 0, 0, 0);
		return output;
	}

	DrawCommand dcmd = g_cmd_list.Load<DrawCommand>(drawCmdListItemOffset(instance_index));

	float3 pos_camworld = dcmd.chunk_base_camworld;
	if ((vertex_index & 1) != 0) pos_camworld.z += dcmd.chunk_size_metres;
	if ((vertex_index & 2) != 0) pos_camworld.x += dcmd.chunk_size_metres;
	if ((vertex_index & 4) != 0) pos_camworld.y += dcmd.chunk_size_metres;

	position = mul(g_main_scene.translated_world_to_clip, float4(pos_camworld, 1.0));
	// TODO: need scale, not chunk size!
	output.color = calcColor(dcmd.chunk_size_metres / 24.0);
	return output;
}

[shader("fragment")]
float4 fragmentMain(Varyings input) : SV_Target
{
	return float4(input.color, 1.0);
}
