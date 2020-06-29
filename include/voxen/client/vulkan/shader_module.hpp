#pragma once

#include <voxen/client/vulkan/common.hpp>

namespace voxen::client
{

class VulkanShaderModule {
public:
	explicit VulkanShaderModule(const char *path);
	VulkanShaderModule(VulkanShaderModule &&) = delete;
	VulkanShaderModule(const VulkanShaderModule &) = delete;
	VulkanShaderModule &operator = (VulkanShaderModule &&) = delete;
	VulkanShaderModule &operator = (const VulkanShaderModule &) = delete;
	~VulkanShaderModule() noexcept;

	operator VkShaderModule() const noexcept { return m_shader_module; }
private:
	VkShaderModule m_shader_module = VK_NULL_HANDLE;
};

class VulkanShaderModuleCollection {
public:
	VulkanShaderModuleCollection();
	VulkanShaderModuleCollection(VulkanShaderModuleCollection &&) = delete;
	VulkanShaderModuleCollection(const VulkanShaderModuleCollection &) = delete;
	VulkanShaderModuleCollection &operator = (VulkanShaderModuleCollection &&) = delete;
	VulkanShaderModuleCollection &operator = (const VulkanShaderModuleCollection &) = delete;
	~VulkanShaderModuleCollection() = default;

	VulkanShaderModule &debugOctreeVertexShader() noexcept { return m_debug_octree_vertex; }
	VulkanShaderModule &debugOctreeFragmentShader() noexcept { return m_debug_octree_fragment; }
private:
	VulkanShaderModule m_debug_octree_vertex;
	VulkanShaderModule m_debug_octree_fragment;
};

}
