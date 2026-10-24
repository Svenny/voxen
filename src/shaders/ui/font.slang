module font;

import util.color_ops;

// TODO: unify with CPU code
struct GlyphCommand {
	float2 up_left_pos;
	float2 lo_right_pos;
	float2 up_left_uv;
	float2 lo_right_uv;
	uint8_t4 color_srgb;
};

struct PushConstants {
	float2 inv_screen_size;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> g_push_const;
[[vk::binding(0, 0)]] Sampler2D g_font_atlas;
[[vk::binding(1, 0)]] StructuredBuffer<GlyphCommand> g_glyph_commands;

struct Varyings {
	[[vk::location(0)]] float2 atlas_uv;
	[[vk::location(1)]] nointerpolation float3 color_linear;
};

float3 unpackColorSrgb(uint8_t4 color_srgb)
{
	return srgbToLinear(float3(color_srgb.rgb) / 255.0);
}

bool2 calcQuadSelector(uint vertex_index)
{
	// Two counter-clockwise (on screen, Y down) triangles
	bool bx = (vertex_index == 2 || vertex_index == 3 || vertex_index == 5);
	bool by = (vertex_index == 1 || vertex_index == 4 || vertex_index == 5);
	return bool2(bx, by);
}

[shader("vertex")]
Varyings vertexMain(uint vertex_index: SV_VulkanVertexID, uint instance_index: SV_VulkanInstanceID,
	out float4 position: SV_Position)
{
	GlyphCommand cmd = g_glyph_commands[instance_index];
	bool2 quad_selector = calcQuadSelector(vertex_index);

	float2 screen_pos = select(quad_selector, cmd.lo_right_pos, cmd.up_left_pos);

	position.xy = mad(screen_pos * g_push_const.inv_screen_size, float2(2.0), float2(-1.0));
	position.zw = float2(0, 1);

	Varyings output;
	output.atlas_uv = select(quad_selector, cmd.lo_right_uv, cmd.up_left_uv);
	output.color_linear = unpackColorSrgb(cmd.color_srgb);
	return output;
}

static const float SMOOTHNESS = 0.175;

[shader("fragment")]
float4 fragmentMain(Varyings input) : SV_Target
{
	float sdf = g_font_atlas.Sample(input.atlas_uv).x;
	float alpha = smoothstep(0.5 - SMOOTHNESS, 0.5 + SMOOTHNESS, sdf);
	return float4(input.color_linear, alpha);
}
