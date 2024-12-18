#version 460 core

layout(push_constant) uniform PushConstants {
	vec3 cube_pos_camworld;
	float cube_size_world;
} g_push_const;

layout(set = 0, binding = 0, std140) uniform CameraParameters {
	mat4 translated_world_to_clip;
	vec3 world_position; float _pad0;
} g_ubo_cam_params;

layout(location = 0) out vec3 out_cube_vector;

const float EXPANSION = 0.1;

void main()
{
	vec3 pos_offset = vec3(0, 0, 0);
	vec3 cube_vector = vec3(-1, -1, -1);

	if ((gl_VertexIndex & 1u) != 0u) {
		pos_offset.z = g_push_const.cube_size_world;
		cube_vector.z = 1.0;
	}

	if ((gl_VertexIndex & 2u) != 0u) {
		pos_offset.x = g_push_const.cube_size_world;
		cube_vector.x = 1.0;
	}

	if ((gl_VertexIndex & 4u) != 0u) {
		pos_offset.y = g_push_const.cube_size_world;
		cube_vector.y = 1.0;
	}

	// Expand the selection cube to be slightly bigger than the covered block
	vec3 pos_camworld = g_push_const.cube_pos_camworld - EXPANSION * g_push_const.cube_size_world + pos_offset * (1.0 + EXPANSION + EXPANSION);

	gl_Position = g_ubo_cam_params.translated_world_to_clip * vec4(pos_camworld, 1);
	out_cube_vector = cube_vector;
}
