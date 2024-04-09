#pragma once

#include <voxen/client/vulkan/common.hpp>

namespace voxen::client::vulkan
{

class Semaphore {
public:
	explicit Semaphore();
	Semaphore(Semaphore &&) = delete;
	Semaphore(const Semaphore &) = delete;
	Semaphore &operator=(Semaphore &&) = delete;
	Semaphore &operator=(const Semaphore &) = delete;
	~Semaphore() noexcept;

	operator VkSemaphore() const noexcept { return m_semaphore; }

private:
	VkSemaphore m_semaphore = VK_NULL_HANDLE;
};

class Fence {
public:
	explicit Fence(bool create_signaled = false);
	Fence(Fence &&) = delete;
	Fence(const Fence &) = delete;
	Fence &operator=(Fence &&) = delete;
	Fence &operator=(const Fence &) = delete;
	~Fence() noexcept;

	operator VkFence() const noexcept { return m_fence; }

private:
	VkFence m_fence = VK_NULL_HANDLE;
};

} // namespace voxen::client::vulkan
