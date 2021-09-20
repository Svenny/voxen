#version 460 core

layout(push_constant, std140) uniform PushConstants {
	vec4 debug_color;
} g_push_const;

layout(location = 0) out vec4 out_color;

void main()
{
	out_color = g_push_const.debug_color;
}
