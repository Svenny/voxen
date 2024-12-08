#version 460 core

#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types : require

#include <land/mesh_layouts.glsl>

layout(constant_id = 0) const uint CHUNK_SIZE_BLOCKS = 32u;
layout(constant_id = 1) const float BLOCK_SIZE_METRES = 0.75f;
layout(constant_id = 2) const uint FAKE_FACE_BATCH_SIZE = 64u;

struct DrawCommand {
	PseudoChunkSurfacePositionRef pos_data;
	PseudoChunkSurfaceAttributesRef attrib_data;

	vec3 chunk_base_camworld;
	float chunk_scale_mult;
};

vec3 calcVertexCamWorldPosition(u16vec3 packed_position, vec3 base_camworld, float scale_mult)
{
	vec3 unpacked_position = vec3(packed_position) / 65535.0;
	return base_camworld + unpacked_position * scale_mult * float(CHUNK_SIZE_BLOCKS) * BLOCK_SIZE_METRES;
}

vec3 unpackVertexNormal(i16vec3 packed_normal)
{
	return vec3(packed_normal) / 32767.0;
}

layout(set = 0, binding = 0, std140) uniform CameraParameters {
	mat4 translated_world_to_clip;
	vec3 world_position; float _pad0;
} g_ubo_cam_params;

layout(set = 1, binding = 0, scalar) readonly buffer CmdList
{
	DrawCommand item[];
} g_cmdlist;

layout(location = 0) out vec3 out_normal;
layout(location = 1) out vec4 out_color;

vec4 getColor(float scale_mult)
{
	if (scale_mult <= 1.0) {
		return vec4(1, 0, 0, 1);
	}
	if (scale_mult <= 2.0) {
		return vec4(0.5, 0.5, 0, 1);
	}
	if (scale_mult <= 4.0) {
		return vec4(0, 1, 0, 1);
	}
	if (scale_mult <= 8.0) {
		return vec4(0, 0.5, 0.5, 1);
	}
	if (scale_mult <= 16.0) {
		return vec4(0, 0, 1, 1);
	}
	if (scale_mult <= 32.0) {
		return vec4(0.5, 0, 0.5, 1);
	}
	if (scale_mult <= 64.0) {
		return vec4(0.35, 0.45, 0.55, 1);
	}
	if (scale_mult <= 128.0) {
		return vec4(0.45, 0.6, 0.75, 1);
	}
	return vec4(0.55, 0.75, 0.9, 1);
}

void main()
{
	DrawCommand dcmd = g_cmdlist.item[gl_DrawID];

	u16vec3 packed_vertex_position = dcmd.pos_data[gl_VertexIndex].position_unorm;
	i16vec3 packed_vertex_normal = dcmd.attrib_data[gl_VertexIndex].normal_snorm;

	vec3 pos_camworld = calcVertexCamWorldPosition(packed_vertex_position, dcmd.chunk_base_camworld, dcmd.chunk_scale_mult);
	vec3 normal_world = unpackVertexNormal(packed_vertex_normal);

	gl_Position = g_ubo_cam_params.translated_world_to_clip * vec4(pos_camworld, 1);
	//out_normal = (g_ubo_cam_params.translated_world_to_clip * vec4(normal_world, 0)).xyz;
	out_normal = normal_world;
	out_color = getColor(dcmd.chunk_scale_mult);
}
