#version 460 core

#extension GL_EXT_buffer_reference2 : require

#include <land/mesh_layouts.glsl>
#include <util/color_ops.glsl>

struct DrawCommand {
	PseudoChunkSurfacePositionRef pos_data;
	PseudoChunkSurfaceAttributesRef attrib_data;

	vec3 chunk_base_camworld;
	float chunk_size_metres;
};

vec3 unpackVertexPosition(u16vec3 packed_position)
{
	vec3 unpacked_position = vec3(packed_position) / 65535.0;
	return unpacked_position * 1.25 - 0.125;
}

vec3 calcVertexCamWorldPosition(vec3 unpacked_position, vec3 base_camworld, float chunk_size_metres)
{
	return base_camworld + unpacked_position * chunk_size_metres;
}

vec2 signNonzero(vec2 v)
{
	return vec2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
}

vec3 unpackVertexNormal(i16vec2 packed_normal)
{
	vec2 oct_packed = vec2(packed_normal) / 32767.0;

	vec3 v = vec3(oct_packed.x, 1.0 - abs(oct_packed.x) - abs(oct_packed.y), oct_packed.y);
	if (v.y < 0.0) {
		v.xz = (1.0 - abs(v.zx)) * signNonzero(v.xz);
	}

	return v;
}

layout(set = 0, binding = 0, std140) uniform CameraParameters {
	mat4 translated_world_to_clip;
	vec3 world_position; float _pad0;
} g_ubo_cam_params;

layout(set = 1, binding = 0, scalar) readonly buffer CmdList {
	uint valid_cmds_count;
	uint _pad0;
	uint _pad1;
	uint _pad2;
	DrawCommand item[];
} g_cmdlist;

layout(location = 0) out vec3 out_normal;
layout(location = 1) out vec4 out_color;

vec3 unpackRgb555(uint packed)
{
	if (packed < (1u << 15)) {
		// TODO: values without the highest bit set indicate material, not a fixed color
		return vec3(0, 0, 0);
	}

	vec3 srgb = vec3(float(bitfieldExtract(packed, 10, 5)) / 32.0,
		float(bitfieldExtract(packed, 5, 5)) / 32.0,
		float(bitfieldExtract(packed, 0, 5)) / 32.0);
	return srgbToLinear(srgb);
}

vec4 calcColor(u16vec4 mat_hist_entries, u8vec4 mat_hist_weights)
{
	vec4 sum = vec4(0.0);

	float w = float(mat_hist_weights.x) / 255.0;
	sum += vec4(unpackRgb555(mat_hist_entries.x) * w, w);

	w = float(mat_hist_weights.y) / 255.0;
	sum += vec4(unpackRgb555(mat_hist_entries.y) * w, w);

	w = float(mat_hist_weights.z) / 255.0;
	sum += vec4(unpackRgb555(mat_hist_entries.z) * w, w);

	w = float(mat_hist_weights.w) / 255.0;
	sum += vec4(unpackRgb555(mat_hist_entries.w) * w, w);

	return sum / sum.w;
}

void main()
{
	DrawCommand dcmd = g_cmdlist.item[gl_DrawID];

	u16vec3 packed_vertex_position = dcmd.pos_data[gl_VertexIndex].position_unorm;
	i16vec2 packed_vertex_normal = dcmd.attrib_data[gl_VertexIndex].normal_oct_snorm;

	vec3 pos_chunk_local = unpackVertexPosition(packed_vertex_position);
	vec3 pos_camworld = calcVertexCamWorldPosition(pos_chunk_local, dcmd.chunk_base_camworld, dcmd.chunk_size_metres);
	vec3 normal_world = unpackVertexNormal(packed_vertex_normal);

	gl_Position = g_ubo_cam_params.translated_world_to_clip * vec4(pos_camworld, 1);
	//out_normal = (g_ubo_cam_params.translated_world_to_clip * vec4(normal_world, 0)).xyz;
	out_normal = normal_world;

	out_color = calcColor(dcmd.attrib_data[gl_VertexIndex].mat_hist_entries,
		dcmd.attrib_data[gl_VertexIndex].mat_hist_weights);
}
