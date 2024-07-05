#include <voxen/client/vulkan/sync.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/gfx/vk/vk_device.hpp>

namespace voxen::client::vulkan
{

Semaphore::Semaphore()
{
	VkSemaphoreCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	auto &backend = Backend::backend();
	VkDevice device = backend.device().handle();
	VkResult result = backend.vkCreateSemaphore(device, &info, HostAllocator::callbacks(), &m_semaphore);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkCreateSemaphore");
	}
}

Semaphore::~Semaphore() noexcept
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device().handle();
	backend.vkDestroySemaphore(device, m_semaphore, HostAllocator::callbacks());
}

Fence::Fence(bool create_signaled)
{
	const VkFenceCreateInfo info {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.pNext = nullptr,
		.flags = create_signaled ? VK_FENCE_CREATE_SIGNALED_BIT : VkFenceCreateFlags(0),
	};

	auto &backend = Backend::backend();
	VkDevice device = backend.device().handle();
	VkResult result = backend.vkCreateFence(device, &info, HostAllocator::callbacks(), &m_fence);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkCreateFence");
	}
}

Fence::~Fence() noexcept
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device().handle();
	backend.vkDestroyFence(device, m_fence, HostAllocator::callbacks());
}

} // namespace voxen::client::vulkan
