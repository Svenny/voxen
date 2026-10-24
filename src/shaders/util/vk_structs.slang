module vk_structs;

// Mirroring definitions for CPU-side Vulkan structures

public struct VkDrawIndexedIndirectCommand {
	uint indexCount;
	uint instanceCount;
	uint firstIndex;
	int vertexOffset;
	uint firstInstance;
};
