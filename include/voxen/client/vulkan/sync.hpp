#pragma once

#include <voxen/client/vulkan/common.hpp>

namespace voxen::client
{

class VulkanSemaphore {
public:
	explicit VulkanSemaphore();
	VulkanSemaphore(VulkanSemaphore &&) = delete;
	VulkanSemaphore(const VulkanSemaphore &) = delete;
	VulkanSemaphore &operator = (VulkanSemaphore &&) = delete;
	VulkanSemaphore &operator = (const VulkanSemaphore &) = delete;
	~VulkanSemaphore() noexcept;

	operator VkSemaphore() const noexcept { return m_semaphore; }
private:
	VkSemaphore m_semaphore;
};

class VulkanFence {
public:
	explicit VulkanFence(bool create_signaled = false);
	VulkanFence(VulkanFence &&) = delete;
	VulkanFence(const VulkanFence &) = delete;
	VulkanFence &operator = (VulkanFence &&) = delete;
	VulkanFence &operator = (const VulkanFence &) = delete;
	~VulkanFence() noexcept;

	operator VkFence() const noexcept { return  m_fence; }
private:
	VkFence m_fence;
};

}
