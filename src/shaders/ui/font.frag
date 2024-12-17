#version 460 core

layout(set = 0, binding = 0) uniform sampler2D g_font_atlas;

layout(location = 0) in smooth vec2 in_atlas_uv;
layout(location = 1) in flat vec3 in_color_linear;

layout(location = 0) out vec4 out_color;

const float SMOOTHNESS = 0.175;

void main()
{
	float sdf = texture(g_font_atlas, in_atlas_uv).x;
	float alpha = smoothstep(0.5 - SMOOTHNESS, 0.5 + SMOOTHNESS, sdf);
	out_color = vec4(in_color_linear, alpha);
}
