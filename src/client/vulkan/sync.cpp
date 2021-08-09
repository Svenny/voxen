#include <voxen/client/vulkan/sync.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>

namespace voxen::client::vulkan
{

Semaphore::Semaphore() {
	VkSemaphoreCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	auto &backend = Backend::backend();
	VkDevice device = backend.device();
	VkResult result = backend.vkCreateSemaphore(device, &info, HostAllocator::callbacks(), &m_semaphore);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkCreateSemaphore");
	}
}

Semaphore::~Semaphore() noexcept {
	auto &backend = Backend::backend();
	VkDevice device = backend.device();
	backend.vkDestroySemaphore(device, m_semaphore, HostAllocator::callbacks());
}

Fence::Fence(bool create_signaled) {
	VkFenceCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	if (create_signaled) {
		info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	}

	auto &backend = Backend::backend();
	VkDevice device = backend.device();
	VkResult result = backend.vkCreateFence(device, &info, HostAllocator::callbacks(), &m_fence);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkCreateFence");
	}
}

Fence::~Fence() noexcept {
	auto &backend = Backend::backend();
	VkDevice device = backend.device();
	backend.vkDestroyFence(device, m_fence, HostAllocator::callbacks());
}

}
