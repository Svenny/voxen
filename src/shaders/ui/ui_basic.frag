#version 460 core

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types : require

// TODO: unify with CPU code
struct PerItemData {
	vec2 scissor_min;
	vec2 scissor_max;
};

layout(set = 0, binding = 1, scalar) restrict readonly buffer SsboPerItemData {
	PerItemData data[];
} g_per_item_data;

layout(location = 0) in flat uint32_t in_item_id;
layout(location = 1) in smooth vec2 in_uv;
layout(location = 2) in smooth vec4 in_color_linear;

layout(location = 0) out vec4 out_color;

void main()
{
	PerItemData per_item = g_per_item_data.data[in_item_id];
	vec2 pos = gl_FragCoord.xy;

	if (any(lessThan(pos, per_item.scissor_min)) || any(greaterThan(pos, per_item.scissor_max))) {
		discard;
	}

	out_color = in_color_linear;
}
