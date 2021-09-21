#version 460 core

layout(set = 0, binding = 0, std140) uniform CameraParameters {
	mat4 translated_world_to_clip;
	vec3 world_position; float _pad0;
} g_ubo_cam_params;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_chunk_base_scale;

layout(location = 0) out vec3 out_color;

vec3 calcColor(float scale)
{
	float green = min(log2(scale), 8.0) / 8.0;
	return vec3(1.0 - green, green, 0.0);
}

void main()
{
	vec4 base_scale = in_chunk_base_scale;
	vec3 pos = fma(in_position, base_scale.www, base_scale.xyz);

	gl_Position = g_ubo_cam_params.translated_world_to_clip * vec4(pos, 1.0);
	out_color = calcColor(base_scale.w);
}
