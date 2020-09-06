#version 460 core

layout(location = 0) in vec3 in_normal;

layout(push_constant) uniform PushConstants {
	mat4 in_mtx;
	vec3 in_sun_direction;
};

layout(location = 0) out vec4 out_color;

void main()
{
	vec3 n = normalize(in_normal);
	float d = max(dot(n, in_sun_direction), 0.0);
	vec3 rgb = 0.3 * abs(n) + 0.7 * d * vec3(1.0);
	out_color = vec4(rgb, 1.0);
}
