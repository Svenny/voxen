#ifndef VX_UTIL_VK_STRUCTS_GLSL
#define VX_UTIL_VK_STRUCTS_GLSL

// Mirroring definitions for CPU-side Vulkan structures

struct VkDrawIndexedIndirectCommand {
	uint indexCount;
	uint instanceCount;
	uint firstIndex;
	int vertexOffset;
	uint firstInstance;
};

#endif // VX_UTIL_VK_STRUCTS_GLSL
