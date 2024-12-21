#version 460 core

#extension GL_EXT_buffer_reference2 : require

#include <land/mesh_layouts.glsl>

struct DrawCommand {
	PseudoChunkSurfacePositionRef pos_data;
	PseudoChunkSurfaceAttributesRef attrib_data;

	vec3 chunk_base_camworld;
	float chunk_size_metres;
};

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

layout(location = 0) out vec3 out_color;

vec3 calcColor(float scale)
{
	float green = min(log2(scale), 8.0) / 8.0;
	return vec3(1.0 - green, green, 0.0);
}

void main()
{
	if (gl_InstanceIndex >= g_cmdlist.valid_cmds_count) {
		gl_Position = vec4(0, 0, 0, 0);
		return;
	}

	DrawCommand dcmd = g_cmdlist.item[gl_InstanceIndex];

	vec3 pos_camworld = dcmd.chunk_base_camworld;
	if ((gl_VertexIndex & 1u) != 0u) pos_camworld.z += dcmd.chunk_size_metres;
	if ((gl_VertexIndex & 2u) != 0u) pos_camworld.x += dcmd.chunk_size_metres;
	if ((gl_VertexIndex & 4u) != 0u) pos_camworld.y += dcmd.chunk_size_metres;

	gl_Position = g_ubo_cam_params.translated_world_to_clip * vec4(pos_camworld, 1.0);
	// TODO: need scale, not chunk size!
	out_color = calcColor(dcmd.chunk_size_metres / 24.0);
}
