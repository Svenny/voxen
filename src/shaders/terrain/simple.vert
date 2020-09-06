#version 460 core

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;

layout(push_constant) uniform PushConstants {
	mat4 in_mtx;
	vec3 in_sun_direction;
};

layout(location = 0) out vec3 out_normal;

void main()
{
	out_normal = in_normal;
	gl_Position = in_mtx * vec4(in_position, 1.0);
}
