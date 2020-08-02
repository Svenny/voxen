#pragma once

#include <voxen/client/vulkan/common.hpp>

namespace voxen::client::vulkan
{

class ShaderModule {
public:
	explicit ShaderModule(const char *path);
	ShaderModule(ShaderModule &&) = delete;
	ShaderModule(const ShaderModule &) = delete;
	ShaderModule &operator = (ShaderModule &&) = delete;
	ShaderModule &operator = (const ShaderModule &) = delete;
	~ShaderModule() noexcept;

	operator VkShaderModule() const noexcept { return m_shader_module; }
private:
	VkShaderModule m_shader_module = VK_NULL_HANDLE;
};

class ShaderModuleCollection {
public:
	ShaderModuleCollection();
	ShaderModuleCollection(ShaderModuleCollection &&) = delete;
	ShaderModuleCollection(const ShaderModuleCollection &) = delete;
	ShaderModuleCollection &operator = (ShaderModuleCollection &&) = delete;
	ShaderModuleCollection &operator = (const ShaderModuleCollection &) = delete;
	~ShaderModuleCollection() = default;

	ShaderModule &debugOctreeVertexShader() noexcept { return m_debug_octree_vertex; }
	ShaderModule &debugOctreeFragmentShader() noexcept { return m_debug_octree_fragment; }
private:
	ShaderModule m_debug_octree_vertex;
	ShaderModule m_debug_octree_fragment;
};

}
