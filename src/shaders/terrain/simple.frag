#version 460 core

#include <util/dither.glsl>

layout(push_constant, std140) uniform PushConstants {
	vec3 sun_direction;
} g_push_const;

layout(set = 0, binding = 0, std140) uniform CameraParameters {
	mat4 translated_world_to_clip;
	vec3 world_position; float _pad0;
} g_ubo_cam_params;

layout(location = 0) in smooth vec3 in_normal;
layout(location = 1) in flat uvec3 in_materials;
layout(location = 2) in smooth vec3 in_material_weights;

layout(location = 0) out vec4 out_color;

const vec3 ambient = vec3(0.005);

const vec3 objectColor[] = {
	vec3(0.64, 0.64, 0.64),      // 0 - air
	vec3(59, 23, 3) / 255.0,     // 1 - ground
	vec3(24, 133, 9) / 255.0,    // 2 - grass
	vec3(163, 163, 163) / 255.0, // 3 - rock
	vec3(240, 240, 240) / 255.0, // 4 - snow
	vec3(173, 165, 47) / 255.0,  // 5 - sand
	vec3(87, 86, 79) / 255.0,    // 6 - clay
	vec3(130, 74, 10) / 255.0,   // 7 - iron ore
	vec3(130, 59, 9) / 255.0,    // 8 - copper ore
	vec3(1, 2, 2) / 255.0,       // 9 - coal
	vec3(232, 210, 7) / 255.0,   // 10 - gold ore
	vec3(143, 142, 136) / 255.0, // 11 - wolfram ore
	vec3(140, 138, 116) / 255.0, // 12 - titanium ore
	vec3(4, 66, 8) / 255.0,      // 13 - uranium ore
};

void main()
{
	vec3 w = in_material_weights;
	vec3 c1 = objectColor[in_materials.x];
	vec3 c2 = objectColor[in_materials.y];
	vec3 c3 = objectColor[in_materials.z];
	vec3 color = (c1 * w.x + c2 * w.y + c3 * w.z) / (w.x + w.y + w.z);

	vec3 n = normalize(in_normal);
	vec3 diffuse = vec3(max(dot(n, g_push_const.sun_direction), 0.0));
	out_color = vec4((ambient + diffuse) * color, 1.0);

	out_color.rgb = dither256(out_color.rgb, uvec2(gl_FragCoord.xy));
}
