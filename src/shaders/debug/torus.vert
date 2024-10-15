#version 460 core

layout(set = 0, binding = 0, std140) uniform CameraParameters {
	mat4 translated_world_to_clip;
	vec3 world_position; float _pad0;
} g_ubo_cam_params;

layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec3 out_normal;

const uint NUM_QUADS_W = 128;
const uint NUM_QUADS_H = 64;

const float CENTER_RADIUS = 1428.5f;
const float VERTICAL_RADIUS = 565.2f;
const float HORIZONTAL_RADIUS = 407.0f;

const float TWOPI = 6.2831855f;

void main()
{
	uint quad_id = gl_VertexIndex / 6u;
	uint in_quad_id = gl_VertexIndex % 6u;

	uint grid_x = quad_id % NUM_QUADS_W;
	uint grid_y = quad_id / NUM_QUADS_W;

	// 0-1
	// |/|
	// 2-3

	// 0-1.3
	// |/./|
	// 2.5-4
	switch (in_quad_id) {
	case 1:
	case 3:
		grid_x++;
		break;
	case 2:
	case 5:
		grid_y++;
		break;
	case 4:
		grid_x++;
		grid_y++;
		break;
	}

	vec2 uv = vec2(grid_x, grid_y) / vec2(NUM_QUADS_W - 1, NUM_QUADS_H - 1);

	// Direction towards the closest point on the center radius line
	vec3 cr_dir = vec3(cos(TWOPI * uv.x), 0, sin(TWOPI * uv.x));
	// Closest point on the center radius line
	vec3 cr = CENTER_RADIUS * cr_dir;

	vec3 point_dir = sin(TWOPI * uv.y) * vec3(0, 1, 0) - cos(TWOPI * uv.y) * cr_dir;
	vec3 point = cr + vec3(HORIZONTAL_RADIUS, VERTICAL_RADIUS, HORIZONTAL_RADIUS) * point_dir + vec3(0, 1000, 0);

	gl_Position = g_ubo_cam_params.translated_world_to_clip * vec4(point - g_ubo_cam_params.world_position, 1);
	out_uv = uv;
	out_normal = point_dir;
}
