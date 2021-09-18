#version 460 core

#include <util/dither.glsl>

layout(push_constant, std140) uniform PushConstants {
	vec4 chunk_base_scale;
	vec3 sun_direction; float _pad0;
} g_push_const;

layout(set = 0, binding = 0, std140) uniform CameraParameters {
	mat4 translated_world_to_clip;
	vec3 world_position; float _pad0;
} g_ubo_cam_params;

layout(location = 0) in vec3 in_normal;

layout(location = 0) out vec4 out_color;

const vec3 ambient = vec3(0.005);
const vec3 objectColor = vec3(0.64, 0.64, 0.64);

void main()
{
	vec3 n = normalize(in_normal);
	vec3 diffuse = vec3(max(dot(n, g_push_const.sun_direction), 0.0));
	out_color = vec4((ambient + diffuse) * objectColor, 1.0);

	out_color.rgb = dither256(out_color.rgb, uvec2(gl_FragCoord.xy));
}
