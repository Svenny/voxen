module ui_basic;

import util.color_ops;

// TODO: unify with CPU code
struct VertexData {
	float2 position;
	float2 uv;
	uint8_t4 color_srgb;
	uint item_id;
};

// TODO: unify with CPU code
struct PerItemData {
	float2 scissor_min;
	float2 scissor_max;
};

struct PushConstants {
	float2 inv_screen_size;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> g_push_const;
[[vk::binding(0, 0)]] StructuredBuffer<VertexData> g_vertex_buffer;
[[vk::binding(1, 0)]] StructuredBuffer<PerItemData> g_per_item_data;

struct Varyings {
	[[vk::location(0)]] nointerpolation uint item_id;
	[[vk::location(1)]] float2 uv;
	[[vk::location(2)]] float4 color_linear;
};

float4 unpackColorSrgb(uint8_t4 color_srgb)
{
	return float4(srgbToLinear(float3(color_srgb.rgb) / 255.0), float(color_srgb.a) / 255.0);
}

[shader("vertex")]
Varyings vertexMain(uint vertex_index: SV_VulkanVertexID, out float4 position: SV_Position)
{
	VertexData vertex_data = g_vertex_buffer[vertex_index];

	position.xy = mad(vertex_data.position * g_push_const.inv_screen_size, float2(2.0), float2(-1.0));
	position.zw = float2(0, 1);

	Varyings output;
	output.item_id = vertex_data.item_id;
	output.uv = vertex_data.uv;
	output.color_linear = unpackColorSrgb(vertex_data.color_srgb);
	return output;
}

[shader("fragment")]
float4 fragmentMain(Varyings input, float4 frag_coord: SV_Position) : SV_Target
{
	PerItemData per_item = g_per_item_data[input.item_id];
	float2 pos = frag_coord.xy;

	if (any(pos < per_item.scissor_min) || any(pos > per_item.scissor_max)) {
		discard;
	}

	return input.color_linear;
}
