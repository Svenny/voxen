#pragma once

#include <voxen/gfx/gfx_fwd.hpp>
#include <voxen/gfx/vk/vk_include.hpp>

namespace voxen::gfx::vk
{

// Hardcoded set of pipeline layouts used by legacy render passes.
// TODO: legacy part, layouts should be created by their users or by a pipeline factory.
class PipelineLayoutCollection {
public:
	explicit PipelineLayoutCollection(Device &dev, const DescriptorSetLayoutCollection &dsls);
	PipelineLayoutCollection(PipelineLayoutCollection &&) = delete;
	PipelineLayoutCollection(const PipelineLayoutCollection &) = delete;
	PipelineLayoutCollection &operator=(PipelineLayoutCollection &&) = delete;
	PipelineLayoutCollection &operator=(const PipelineLayoutCollection &) = delete;
	~PipelineLayoutCollection() noexcept;

	VkPipelineLayout landFrustumCullLayout() const noexcept { return m_land_frustum_cull_layout; }
	VkPipelineLayout landChunkMeshLayout() const noexcept { return m_land_chunk_mesh_layout; }
	VkPipelineLayout landSelectorLayout() const noexcept { return m_land_selector_layout; }
	VkPipelineLayout uiFontLayout() const noexcept { return m_ui_font_layout; }
	VkPipelineLayout uiBasicLayout() const noexcept { return m_ui_basic_layout; }

private:
	Device &m_dev;

	VkPipelineLayout m_land_frustum_cull_layout = VK_NULL_HANDLE;
	VkPipelineLayout m_land_chunk_mesh_layout = VK_NULL_HANDLE;
	VkPipelineLayout m_land_selector_layout = VK_NULL_HANDLE;
	VkPipelineLayout m_ui_font_layout = VK_NULL_HANDLE;
	VkPipelineLayout m_ui_basic_layout = VK_NULL_HANDLE;

	void destroyLayouts() noexcept;
};

} // namespace voxen::gfx::vk
