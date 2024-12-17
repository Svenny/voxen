#pragma once

#include <voxen/gfx/vk/vk_include.hpp>

namespace voxen::client::vulkan
{

class PipelineLayout {
public:
	PipelineLayout(const VkPipelineLayoutCreateInfo &info);
	PipelineLayout(PipelineLayout &&) = delete;
	PipelineLayout(const PipelineLayout &) = delete;
	PipelineLayout &operator=(PipelineLayout &&) = delete;
	PipelineLayout &operator=(const PipelineLayout &) = delete;
	~PipelineLayout() noexcept;

	operator VkPipelineLayout() const noexcept { return m_layout; }

private:
	VkPipelineLayout m_layout = VK_NULL_HANDLE;
};

class PipelineLayoutCollection {
public:
	PipelineLayoutCollection();
	PipelineLayoutCollection(PipelineLayoutCollection &&) = delete;
	PipelineLayoutCollection(const PipelineLayoutCollection &) = delete;
	PipelineLayoutCollection &operator=(PipelineLayoutCollection &&) = delete;
	PipelineLayoutCollection &operator=(const PipelineLayoutCollection &) = delete;
	~PipelineLayoutCollection() = default;

	PipelineLayout &terrainBasicLayout() noexcept { return m_terrain_basic_layout; }
	PipelineLayout &terrainFrustumCullLayout() noexcept { return m_terrain_frustum_cull_layout; }
	PipelineLayout &uiFontLayout() noexcept { return m_ui_font_layout; }

private:
	PipelineLayout m_terrain_basic_layout;
	PipelineLayout m_terrain_frustum_cull_layout;
	PipelineLayout m_ui_font_layout;

	PipelineLayout createTerrainBasicLayout();
	PipelineLayout createTerrainFrustumCullLayout();
	PipelineLayout createUiFontLayout();
};

} // namespace voxen::client::vulkan
