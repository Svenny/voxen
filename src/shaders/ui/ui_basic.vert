#version 460 core

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types : require

#include <util/color_ops.glsl>

// TODO: unify with CPU code
struct VertexData {
	vec2 position;
	vec2 uv;
	u8vec4 color_srgb;
	uint32_t item_id;
};

layout(push_constant) uniform PushConstants {
	vec2 inv_screen_size;
} g_push_const;

layout(set = 0, binding = 0, scalar) restrict readonly buffer SsboVertexData {
	VertexData data[];
} g_vertex_buffer;

vec4 unpackColorSrgb(u8vec4 color_srgb)
{
	return vec4(srgbToLinear(vec3(color_srgb.rgb) / 255.0), float(color_srgb.a) / 255.0);
}

layout(location = 0) out uint32_t out_item_id;
layout(location = 1) out vec2 out_uv;
layout(location = 2) out vec4 out_color_linear;

void main()
{
	VertexData vertex_data = g_vertex_buffer.data[gl_VertexIndex];

	gl_Position.xy = fma(vertex_data.position * g_push_const.inv_screen_size, vec2(2.0), vec2(-1.0));
	gl_Position.zw = vec2(0, 1);

	out_item_id = vertex_data.item_id;
	out_uv = vertex_data.uv;
	out_color_linear = unpackColorSrgb(vertex_data.color_srgb);
}
