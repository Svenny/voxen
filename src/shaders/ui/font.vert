#version 460 core

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types : require

// TODO: unify with CPU code
struct GlyphCommand {
	vec2 up_left_pos;
	vec2 lo_right_pos;
	vec2 up_left_uv;
	vec2 lo_right_uv;
	u8vec4 color_srgb;
};

layout(push_constant) uniform PushConstants {
	vec2 inv_screen_size;
} g_push_const;

layout(set = 0, binding = 1, scalar) readonly buffer SsboGlyphCommands {
	GlyphCommand cmds[];
} g_glyph_commands;

vec3 unpackColorSrgb(u8vec4 color_srgb)
{
	vec3 c = vec3(color_srgb.rgb) / 255.0;

	bvec3 small = lessThanEqual(c, vec3(0.04045));
	vec3 linear_part = c * 12.92;
	vec3 nonlinear_part = pow((c + 0.055) / 1.055, vec3(2.4));

	return mix(nonlinear_part, linear_part, small);
}

bvec2 calcQuadSelector()
{
	uint index = gl_VertexIndex;
	bool bx = (gl_VertexIndex == 1 || gl_VertexIndex == 4 || gl_VertexIndex == 5);
	bool by = (gl_VertexIndex == 2 || gl_VertexIndex == 3 || gl_VertexIndex == 5);
	return bvec2(bx, by);
}

layout(location = 0) out vec2 out_atlas_uv;
layout(location = 1) out vec3 out_color_linear;

void main()
{
	GlyphCommand cmd = g_glyph_commands.cmds[gl_InstanceIndex];
	bvec2 quad_selector = calcQuadSelector();

	vec2 screen_pos = mix(cmd.up_left_pos, cmd.lo_right_pos, quad_selector);

	gl_Position.xy = fma(screen_pos * g_push_const.inv_screen_size, vec2(2.0), vec2(-1.0));
	gl_Position.zw = vec2(0, 1);
	out_atlas_uv = mix(cmd.up_left_uv, cmd.lo_right_uv, quad_selector);
	out_color_linear = unpackColorSrgb(cmd.color_srgb);
}
