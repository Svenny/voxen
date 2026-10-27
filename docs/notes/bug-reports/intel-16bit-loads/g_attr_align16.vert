#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types : require

// 16-byte records, 2-byte aligned 16-bit vector members (scalar layout)
layout(buffer_reference, scalar, buffer_reference_align = 16) readonly buffer Attr {
	i16vec2 a;
	u16vec4 b;
};

layout(set = 1, binding = 0, scalar) readonly buffer Addr { uint64_t addr; } g_addr;

layout(location = 0) out vec3 o_a;
layout(location = 1) out vec4 o_b;

void main()
{
	Attr p = Attr(g_addr.addr + uint64_t(gl_VertexIndex) * 16);
	o_a = vec3(vec2(p.a), 0.0);
	o_b = vec4(p.b);
	gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
}
