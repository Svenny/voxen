module chunk_mesh;

import land.mesh_layouts;
import util.color_ops;

#include "shared/main_scene.h"

[[vk::binding(0, 0)]] ConstantBuffer<MainSceneUbo> g_main_scene;
// Draw command list, indexed by draw ID
[[vk::binding(0, 1)]] ByteAddressBuffer g_cmd_list;

struct Varyings {
	[[vk::location(0)]] float3 normal;
	[[vk::location(1)]] float4 color;
};

float3 unpackVertexPosition(uint16_t3 packed_position)
{
	float3 unpacked_position = float3(packed_position) / 65535.0;
	return unpacked_position * 1.25 - 0.125;
}

float3 calcVertexCamWorldPosition(float3 unpacked_position, float3 base_camworld, float chunk_size_metres)
{
	return base_camworld + unpacked_position * chunk_size_metres;
}

float2 signNonzero(float2 v)
{
	return float2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
}

float3 unpackVertexNormal(int16_t2 packed_normal)
{
	float2 oct_packed = float2(packed_normal) / 32767.0;

	float3 v = float3(oct_packed.x, 1.0 - abs(oct_packed.x) - abs(oct_packed.y), oct_packed.y);
	if (v.y < 0.0) {
		v.xz = (1.0 - abs(v.zx)) * signNonzero(v.xz);
	}

	return v;
}

float3 unpackRgb555(uint packed)
{
	if (packed < (1u << 15)) {
		// TODO: values without the highest bit set indicate material, not a fixed color
		return float3(0, 0, 0);
	}

	float3 srgb = float3(float((packed >> 10) & 0x1F) / 32.0, float((packed >> 5) & 0x1F) / 32.0,
		float(packed & 0x1F) / 32.0);
	return srgbToLinear(srgb);
}

float4 calcColor(uint16_t4 mat_hist_entries, uint8_t4 mat_hist_weights)
{
	float4 sum = float4(0.0);

	float w = float(mat_hist_weights.x) / 255.0;
	sum += float4(unpackRgb555(uint(mat_hist_entries.x)) * w, w);

	w = float(mat_hist_weights.y) / 255.0;
	sum += float4(unpackRgb555(uint(mat_hist_entries.y)) * w, w);

	w = float(mat_hist_weights.z) / 255.0;
	sum += float4(unpackRgb555(uint(mat_hist_entries.z)) * w, w);

	w = float(mat_hist_weights.w) / 255.0;
	sum += float4(unpackRgb555(uint(mat_hist_entries.w)) * w, w);

	return sum / sum.w;
}

[shader("vertex")]
Varyings vertexMain(uint vertex_index: SV_VulkanVertexID, uint draw_index: SV_DrawIndex,
	out float4 position: SV_Position)
{
	DrawCommand dcmd = g_cmd_list.Load<DrawCommand>(drawCmdListItemOffset(draw_index));

	uint16_t3 packed_vertex_position = dcmd.pos_data[vertex_index].position_unorm;
	PseudoChunkSurfaceAttributes attribs = dcmd.attrib_data[vertex_index];

	float3 pos_chunk_local = unpackVertexPosition(packed_vertex_position);
	float3 pos_camworld = calcVertexCamWorldPosition(pos_chunk_local, dcmd.chunk_base_camworld, dcmd.chunk_size_metres);
	float3 normal_world = unpackVertexNormal(attribs.normal_oct_snorm);

	position = mul(g_main_scene.translated_world_to_clip, float4(pos_camworld, 1));

	Varyings output;
	output.normal = normal_world;
	output.color = calcColor(attribs.mat_hist_entries, attribs.mat_hist_weights);
	return output;
}

static const int NUM_SUNS = 2;

static const float3 SUN_DIRECTIONS[NUM_SUNS] = {
	normalize(float3(0.15, 0.7, 0.3)),
	normalize(float3(-0.4, 0.7, -0.1)),
};

static const float3 SUN_TINTS[NUM_SUNS] = {
	float3(1, 1, 0.9),
	float3(0.2, 0.2, 0.3),
};

static const float3 AMBIENT = float3(0.004, 0.005, 0.006);

[shader("fragment")]
float4 fragmentMain(Varyings input) : SV_Target
{
	float3 norm = normalize(input.normal);
	float3 tint = AMBIENT;

	for (int i = 0; i < NUM_SUNS; i++) {
		tint += SUN_TINTS[i] * max(dot(norm, SUN_DIRECTIONS[i]), 0.0);
	}

	return float4(tint * input.color.rgb, input.color.a);
}
