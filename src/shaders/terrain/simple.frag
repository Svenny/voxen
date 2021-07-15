#version 460 core

#include <util/dither.glsl>

layout(location = 0) in vec3 in_normal;

layout(push_constant) uniform PushConstants {
	mat4 in_mtx;
	vec3 in_sun_direction;
};

layout(location = 0) out vec4 out_color;

const vec3 ambient = vec3(0.005);
const vec3 objectColor = vec3(0.64, 0.64, 0.64);

void main()
{
	vec3 n = normalize(in_normal);
	vec3 diffuse = vec3(max(dot(n, in_sun_direction), 0.0));
	out_color = vec4((ambient + diffuse) * objectColor, 1.0);

	out_color.rgb = dither256(out_color.rgb, uvec2(gl_FragCoord.xy));
}
