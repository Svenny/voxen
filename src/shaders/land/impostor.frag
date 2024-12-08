#version 460 core

layout(location = 0) in vec3 in_normal;
layout(location = 1) in vec4 in_color;
layout(location = 0) out vec4 out_color;

const int NUM_SUNS = 2;

const vec3 SUN_DIRECTIONS[NUM_SUNS] = {
	normalize(vec3(0.15, 0.7, 0.3)),
	normalize(vec3(-0.4, 0.7, -0.1)),
};

const vec3 SUN_TINTS[NUM_SUNS] = {
	vec3(1, 1, 0.9),
	vec3(0.2, 0.2, 0.3),
};

const vec3 AMBIENT = vec3(0.004, 0.005, 0.006);

void main()
{
	vec3 norm = normalize(in_normal);
	vec3 tint = AMBIENT;

	for (int i = 0; i < NUM_SUNS; i++) {
		tint += SUN_TINTS[i] * max(dot(norm, SUN_DIRECTIONS[i]), 0.0);
	}

	out_color = vec4(tint * in_color.rgb, in_color.a);
}
