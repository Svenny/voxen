#pragma once

#include <voxen/client/vulkan/common.hpp>
#include <voxen/client/vulkan/config.hpp>

namespace voxen::client::vulkan
{

class WrappedVkDescriptorPool final {
public:
	constexpr explicit WrappedVkDescriptorPool(VkDescriptorPool handle = VK_NULL_HANDLE) noexcept : m_handle(handle) {}
	explicit WrappedVkDescriptorPool(const VkDescriptorPoolCreateInfo &info);
	WrappedVkDescriptorPool(WrappedVkDescriptorPool &&) noexcept;
	WrappedVkDescriptorPool(const WrappedVkDescriptorPool &) = delete;
	WrappedVkDescriptorPool &operator = (WrappedVkDescriptorPool &&) noexcept;
	WrappedVkDescriptorPool &operator = (const WrappedVkDescriptorPool &) = delete;
	~WrappedVkDescriptorPool() noexcept;

	VkDescriptorPool handle() const noexcept { return m_handle; }
	operator VkDescriptorPool() const noexcept { return m_handle; }

private:
	VkDescriptorPool m_handle = VK_NULL_HANDLE;
};

class DescriptorManager final {
public:
	DescriptorManager();
	DescriptorManager(DescriptorManager &&) = delete;
	DescriptorManager(const DescriptorManager &) = delete;
	DescriptorManager &operator = (DescriptorManager &&) = delete;
	DescriptorManager &operator = (const DescriptorManager &) = delete;
	~DescriptorManager() = default;

	void startNewFrame() noexcept { m_set_id = (m_set_id + 1) % Config::NUM_CPU_PENDING_FRAMES; }
	uint32_t setId() const noexcept { return m_set_id; }

	VkDescriptorSet mainSceneSet() const noexcept { return m_main_scene_set[m_set_id]; }
	VkDescriptorSet terrainFrustumCullSet() const noexcept { return m_terrain_frustum_cull_set[m_set_id]; }

private:
	uint32_t m_set_id = 0;
	WrappedVkDescriptorPool m_main_pool;
	VkDescriptorSet m_main_scene_set[Config::NUM_CPU_PENDING_FRAMES];
	VkDescriptorSet m_terrain_frustum_cull_set[Config::NUM_CPU_PENDING_FRAMES];
};

}
