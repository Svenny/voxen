#version 460 core

#extension GL_EXT_shader_explicit_arithmetic_types : require

layout(constant_id = 0) const uint CHUNK_SIZE_BLOCKS = 32u;
layout(constant_id = 1) const float BLOCK_SIZE_METRES = 0.75f;
layout(constant_id = 2) const uint FAKE_FACE_BATCH_SIZE = 64u;

struct RListItem
{
	int32_t chunk_key_x;
	int32_t chunk_key_y;
	int32_t chunk_key_z;
	uint32_t misc;
	uint32_t data_pool_slot;
};

struct FakeFace
{
	uint32_t packed_data;
	uint32_t corner_weights;
	uint32_t packed_color_srgb;
};

layout(set = 0, binding = 0, std140) uniform CameraParameters {
	mat4 translated_world_to_clip;
	vec3 world_position; float _pad0;
} g_ubo_cam_params;

layout(set = 1, binding = 0, std430) readonly buffer RList
{
	RListItem items[];
} g_rlist;

layout(set = 1, binding = 1, std430) readonly buffer FakeFacePool
{
	FakeFace faces[];
} g_fake_face_pool;

layout(location = 0) out vec3 out_normal;
layout(location = 1) out vec4 out_color;

const ivec3 VERTEX_OFFSET[8] = {
	ivec3(0, 0, 0),
	ivec3(0, 0, 1),
	ivec3(1, 0, 0),
	ivec3(1, 0, 1),
	ivec3(0, 1, 0),
	ivec3(0, 1, 1),
	ivec3(1, 1, 0),
	ivec3(1, 1, 1),
};

const uint INDEX_BUFFER[6][6] = {
	{ 3, 2, 6, 6, 7, 3 }, // X+
	{ 4, 0, 1, 1, 5, 4 }, // X-
	{ 6, 4, 5, 5, 7, 6 }, // Y+
	{ 1, 0, 2, 2, 3, 1 }, // Y-
	{ 5, 1, 3, 3, 7, 5 }, // Z+
	{ 2, 0, 4, 4, 6, 2 }, // Z-
};

const vec3 NORMAL_BUFFER[6] = {
	vec3(1, 0, 0), vec3(-1, 0, 0),
	vec3(0, 1, 0), vec3(0, -1, 0),
	vec3(0, 0, 1), vec3(0, 0, -1),
};

vec4 unpackSrgb(uint packed_srgb)
{
	// TODO: no gamma conversion
	return unpackUnorm4x8(packed_srgb);
}

void main()
{
	uint vertex_index = gl_VertexIndex % 6u;
	uint index_in_batch = gl_VertexIndex / 6u;

	RListItem rcmd = g_rlist.items[gl_InstanceIndex];

	if (index_in_batch >= bitfieldExtract(rcmd.misc, 12, 20)) {
		// Degenerate this triangle
		gl_Position = vec4(0);
		return;
	}

	FakeFace face_data = g_fake_face_pool.faces[FAKE_FACE_BATCH_SIZE * bitfieldExtract(rcmd.data_pool_slot, 16, 16) + index_in_batch];

	uint32_t lod = bitfieldExtract(rcmd.misc, 4, 8);
	int32_t cx = rcmd.chunk_key_x * int32_t(CHUNK_SIZE_BLOCKS) + int32_t(bitfieldExtract(face_data.packed_data, 0, 5) << lod);
	int32_t cy = rcmd.chunk_key_y * int32_t(CHUNK_SIZE_BLOCKS) + int32_t(bitfieldExtract(face_data.packed_data, 5, 5) << lod);
	int32_t cz = rcmd.chunk_key_z * int32_t(CHUNK_SIZE_BLOCKS) + int32_t(bitfieldExtract(face_data.packed_data, 10, 5) << lod);

	uint face = bitfieldExtract(face_data.packed_data, 15, 3);
	ivec3 face_step = VERTEX_OFFSET[INDEX_BUFFER[face][vertex_index]] << int32_t(lod);

	vec3 vertex_chunk_coord = vec3(ivec3(cx, cy, cz) + face_step);
	vec3 translated_world_pos = vertex_chunk_coord * BLOCK_SIZE_METRES - g_ubo_cam_params.world_position;

	gl_Position = g_ubo_cam_params.translated_world_to_clip * vec4(translated_world_pos, 1);
	out_normal = NORMAL_BUFFER[face];
	out_color = unpackSrgb(face_data.packed_color_srgb);
}
