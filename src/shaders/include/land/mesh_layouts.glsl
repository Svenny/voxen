#ifndef VX_LAND_MESH_LAYOUTS_GLSL
#define VX_LAND_MESH_LAYOUTS_GLSL

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

// TODO: unify with declaration of `voxen::land::PseudoSurfaceVertexPosition`
layout(buffer_reference, scalar, buffer_reference_align = 2) buffer PseudoChunkSurfacePositionRef {
	u16vec3 position_unorm;
};

// TODO: unify with declaration of `voxen::land::PseudoSurfaceVertexAttributes`
layout(buffer_reference, scalar, buffer_reference_align = 2) buffer PseudoChunkSurfaceAttributesRef {
	i16vec3 normal_snorm;
	u16vec4 histogram_mat_id_or_color;
	u8vec4 histogram_mat_weight;
};

#endif // VX_LAND_MESH_LAYOUTS_GLSL
