#version 460 core

layout(location = 0) in vec3 in_normal;
layout(location = 1) in vec4 in_color;
layout(location = 0) out vec4 out_color;

const vec3 SUN_DIRECTION = normalize(vec3(0.15, 0.7, 0.3));
const vec3 AMBIENT = vec3(0.005);

void main()
{
	float diffuse = max(dot(normalize(in_normal), SUN_DIRECTION), 0.0);
	out_color = vec4((AMBIENT + diffuse) * in_color.rgb, in_color.a);
}
