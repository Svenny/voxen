#pragma once

#include <voxen/client/vulkan/common.hpp>

#include <unordered_map>

namespace voxen::client::vulkan
{

class WrappedVkDescriptorSetLayout final {
public:
	constexpr explicit WrappedVkDescriptorSetLayout(VkDescriptorSetLayout handle = VK_NULL_HANDLE) noexcept
		: m_handle(handle)
	{}
	explicit WrappedVkDescriptorSetLayout(const VkDescriptorSetLayoutCreateInfo &info);
	WrappedVkDescriptorSetLayout(WrappedVkDescriptorSetLayout &&other) noexcept;
	WrappedVkDescriptorSetLayout(const WrappedVkDescriptorSetLayout &) = delete;
	WrappedVkDescriptorSetLayout &operator=(WrappedVkDescriptorSetLayout &&other) noexcept;
	WrappedVkDescriptorSetLayout &operator=(const WrappedVkDescriptorSetLayout &) = delete;
	~WrappedVkDescriptorSetLayout() noexcept;

	VkDescriptorSetLayout handle() const noexcept { return m_handle; }
	operator VkDescriptorSetLayout() const noexcept { return m_handle; }

private:
	VkDescriptorSetLayout m_handle = VK_NULL_HANDLE;
};

class DescriptorSetLayoutCollection final {
public:
	DescriptorSetLayoutCollection();
	DescriptorSetLayoutCollection(DescriptorSetLayoutCollection &&) = delete;
	DescriptorSetLayoutCollection(const DescriptorSetLayoutCollection &) = delete;
	DescriptorSetLayoutCollection &operator=(DescriptorSetLayoutCollection &&) = delete;
	DescriptorSetLayoutCollection &operator=(const DescriptorSetLayoutCollection &) = delete;
	~DescriptorSetLayoutCollection() = default;

	const std::unordered_map<VkDescriptorType, uint32_t> totalDescriptorConsumption() const noexcept
	{
		return m_descriptor_consumption;
	}

	WrappedVkDescriptorSetLayout &mainSceneLayout() noexcept { return m_main_scene_layout; }
	WrappedVkDescriptorSetLayout &terrainFrustumCullLayout() noexcept { return m_terrain_frustum_cull_layout; }

private:
	std::unordered_map<VkDescriptorType, uint32_t> m_descriptor_consumption;
	WrappedVkDescriptorSetLayout m_main_scene_layout;
	WrappedVkDescriptorSetLayout m_terrain_frustum_cull_layout;

	void appendDescriptorConsumption(const VkDescriptorSetLayoutCreateInfo &info);
	WrappedVkDescriptorSetLayout createMainSceneLayout();
	WrappedVkDescriptorSetLayout createTerrainFrustumCullLayout();
};

} // namespace voxen::client::vulkan
