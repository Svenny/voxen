#version 460 core

layout(set = 0, binding = 0, std140) uniform CameraParameters {
	mat4 translated_world_to_clip;
	vec3 world_position; float _pad0;
} g_ubo_cam_params;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_chunk_base_scale;

void main()
{
	vec4 base_scale = in_chunk_base_scale;
	vec3 pos = fma(in_position, base_scale.www, base_scale.xyz);
	gl_Position = g_ubo_cam_params.translated_world_to_clip * vec4(pos, 1.0);
}
