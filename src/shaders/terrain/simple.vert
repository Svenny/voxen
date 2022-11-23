#version 460 core

layout(constant_id = 0) const uint CHUNK_SIZE = 32u;

layout(set = 0, binding = 0, std140) uniform CameraParameters {
	mat4 translated_world_to_clip;
	vec3 world_position; float _pad0;
} g_ubo_cam_params;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in float in_primary_mat;
layout(location = 3) in uint in_flags;
layout(location = 4) in float in_secondary_mats_weight;
layout(location = 5) in float in_secondary_mats_ratio;
layout(location = 6) in float in_secondary_mat_a;
layout(location = 7) in float in_secondary_mat_b;
layout(location = 8) in vec4 in_chunk_base_scale;

layout(location = 0) out vec3 out_normal;

void main()
{
	out_normal = in_normal;

	vec4 base_scale = in_chunk_base_scale;
	vec3 pos = in_position;

	// Slightly expand flanges of non-LOD0 chunks to decrease
	// surface misalignment holes on joints of different LODs
	bool is_flange = (in_flags & 0x01u) != 0u;
	if (base_scale.w > 1.0 && is_flange) {
		const float FLANGE_OFFSET_FACTOR = 1.005;
		const float HALF_CHUNK_SIZE = float(CHUNK_SIZE / 2U);

		// Offset vertex position further from chunk center, making
		// the flange "expand" a bit into neighbouring chunk:
		// (pos - C) * F + C - pos
		// pos * F - C * F + C - pos
		// pos * (F - 1) + C * (1 - F)
		vec3 flange_offset = pos * (FLANGE_OFFSET_FACTOR - 1.0) + HALF_CHUNK_SIZE * (1.0 - FLANGE_OFFSET_FACTOR);
		// Cancel out offset part along surface normal - this greatly decreases expansion holes
		flange_offset -= in_normal * dot(flange_offset, in_normal);

		pos += flange_offset;
	}

	pos = fma(pos, base_scale.www, base_scale.xyz);
	gl_Position = g_ubo_cam_params.translated_world_to_clip * vec4(pos, 1.0);
}
