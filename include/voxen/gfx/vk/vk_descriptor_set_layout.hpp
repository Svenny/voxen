#pragma once

#include <voxen/gfx/gfx_fwd.hpp>
#include <voxen/gfx/vk/vk_include.hpp>

namespace voxen::gfx::vk
{

// Hardcoded set of descriptor set layouts used by legacy render passes.
// TODO: legacy part, layouts should be created by their users or by a pipeline factory.
class DescriptorSetLayoutCollection {
public:
	explicit DescriptorSetLayoutCollection(Device &dev);
	DescriptorSetLayoutCollection(DescriptorSetLayoutCollection &&) = delete;
	DescriptorSetLayoutCollection(const DescriptorSetLayoutCollection &) = delete;
	DescriptorSetLayoutCollection &operator=(DescriptorSetLayoutCollection &&) = delete;
	DescriptorSetLayoutCollection &operator=(const DescriptorSetLayoutCollection &) = delete;
	~DescriptorSetLayoutCollection() noexcept;

	VkDescriptorSetLayout mainSceneLayout() const noexcept { return m_main_scene_layout; }
	VkDescriptorSetLayout landFrustumCullLayout() const noexcept { return m_land_frustum_cull_layout; }
	VkDescriptorSetLayout landChunkMeshLayout() const noexcept { return m_land_chunk_mesh_layout; }
	VkDescriptorSetLayout uiFontLayout() const noexcept { return m_ui_font_layout; }
	VkDescriptorSetLayout uiBasicLayout() const noexcept { return m_ui_basic_layout; }

private:
	Device &m_dev;

	VkDescriptorSetLayout m_main_scene_layout = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_land_frustum_cull_layout = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_land_chunk_mesh_layout = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_ui_font_layout = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_ui_basic_layout = VK_NULL_HANDLE;

	void destroyLayouts() noexcept;
};

} // namespace voxen::gfx::vk
