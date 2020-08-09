#version 460 core
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform PushConstants {
	mat4 in_mtx;
	vec4 in_color;
};

void main() {
	out_color = in_color;
}
