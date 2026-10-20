module selector;

#include "shared/main_scene.h"

struct PushConstants {
	float3 cube_pos_camworld;
	float cube_size_world;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> g_push_const;
[[vk::binding(0, 0)]] ConstantBuffer<MainSceneUbo> g_main_scene;

struct Varyings {
	[[vk::location(0)]] float3 cube_vector;
};

static const float EXPANSION = 0.1;

[shader("vertex")]
Varyings vertexMain(uint vertex_index: SV_VulkanVertexID, out float4 position: SV_Position)
{
	float3 pos_offset = float3(0, 0, 0);
	float3 cube_vector = float3(-1, -1, -1);

	if ((vertex_index & 1) != 0) {
		pos_offset.z = g_push_const.cube_size_world;
		cube_vector.z = 1.0;
	}

	if ((vertex_index & 2) != 0) {
		pos_offset.x = g_push_const.cube_size_world;
		cube_vector.x = 1.0;
	}

	if ((vertex_index & 4) != 0) {
		pos_offset.y = g_push_const.cube_size_world;
		cube_vector.y = 1.0;
	}

	// Expand the selection cube to be slightly bigger than the covered block
	float3 pos_camworld = g_push_const.cube_pos_camworld - EXPANSION * g_push_const.cube_size_world
		+ pos_offset * (1.0 + EXPANSION + EXPANSION);

	position = mul(g_main_scene.translated_world_to_clip, float4(pos_camworld, 1));

	Varyings output;
	output.cube_vector = cube_vector;
	return output;
}

static const float3 COLOR = float3(0.63, 0.67, 0.85);

[shader("fragment")]
float4 fragmentMain(Varyings input) : SV_Target
{
	float3 cv = abs(input.cube_vector);
	cv = cv * cv * cv * cv;
	cv *= cv;

	float alpha = 0.4 * (cv.x + cv.y + cv.z);
	return float4(COLOR, alpha);
}
