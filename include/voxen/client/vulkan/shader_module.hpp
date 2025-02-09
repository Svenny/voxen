#pragma once

#include <voxen/gfx/vk/vk_include.hpp>

#include <array>
#include <string_view>

namespace voxen::client::vulkan
{

class ShaderModule {
public:
	ShaderModule() = default;
	explicit ShaderModule(std::string_view relative_path);
	ShaderModule(ShaderModule &&) = delete;
	ShaderModule(const ShaderModule &) = delete;
	ShaderModule &operator=(ShaderModule &&) = delete;
	ShaderModule &operator=(const ShaderModule &) = delete;
	~ShaderModule() noexcept;

	void load(std::string_view relative_path);
	void unload() noexcept;
	bool isLoaded() const noexcept { return m_shader_module != VK_NULL_HANDLE; }

	operator VkShaderModule() const noexcept { return m_shader_module; }

private:
	VkShaderModule m_shader_module = VK_NULL_HANDLE;
};

class ShaderModuleCollection {
public:
	ShaderModuleCollection();
	ShaderModuleCollection(ShaderModuleCollection &&) = delete;
	ShaderModuleCollection(const ShaderModuleCollection &) = delete;
	ShaderModuleCollection &operator=(ShaderModuleCollection &&) = delete;
	ShaderModuleCollection &operator=(const ShaderModuleCollection &) = delete;
	~ShaderModuleCollection() = default;

	enum ShaderId : uint32_t {
		LAND_DEBUG_CHUNK_BOUNDS_VERTEX,
		LAND_DEBUG_CHUNK_BOUNDS_FRAGMENT,
		LAND_FRUSTUM_CULL_COMPUTE,
		LAND_CHUNK_MESH_VERTEX,
		LAND_CHUNK_MESH_FRAGMENT,
		LAND_SELECTOR_VERTEX,
		LAND_SELECTOR_FRAGMENT,
		UI_FONT_VERTEX,
		UI_FONT_FRAGMENT,

		NUM_SHADERS
	};

	const ShaderModule &operator[](ShaderId idx) const;

private:
	std::array<ShaderModule, NUM_SHADERS> m_shader_modules;
};

} // namespace voxen::client::vulkan
