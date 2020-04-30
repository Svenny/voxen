#version 460 core
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 in_position;

layout(push_constant) uniform PushConstants {
	mat4 in_mtx;
	vec4 in_color;
};

void main() {
	gl_Position = in_mtx * vec4(in_position, 1.0);
}
