#include <voxen/gfx/vk/vk_device.hpp>

#include <voxen/client/vulkan/common.hpp>

namespace voxen::gfx::vk
{

// TODO: there parts are not yet moved to voxen/gfx/vk
using client::vulkan::VulkanException;

VkImageView Device::vkCreateImageView(const VkImageViewCreateInfo &create_info, const char *name, SLoc loc)
{
	VkImageView handle = VK_NULL_HANDLE;
	VkResult res = m_dt.vkCreateImageView(m_handle, &create_info, nullptr, &handle);
	if (res != VK_SUCCESS) {
		throw VulkanException(res, "vkCreateImageView", loc);
	}

	if (name) {
		debug().setObjectName(m_handle, handle, name);
	}

	return handle;
}

VkSemaphore Device::vkCreateSemaphore(const VkSemaphoreCreateInfo &create_info, const char *name, SLoc loc)
{
	VkSemaphore handle = VK_NULL_HANDLE;
	VkResult res = m_dt.vkCreateSemaphore(m_handle, &create_info, nullptr, &handle);
	if (res != VK_SUCCESS) {
		throw VulkanException(res, "vkCreateSemaphore", loc);
	}

	if (name) {
		debug().setObjectName(m_handle, handle, name);
	}

	return handle;
}

VkSwapchainKHR Device::vkCreateSwapchain(const VkSwapchainCreateInfoKHR &create_info, SLoc loc)
{
	VkSwapchainKHR handle = VK_NULL_HANDLE;
	VkResult res = m_dt.vkCreateSwapchainKHR(m_handle, &create_info, nullptr, &handle);
	if (res != VK_SUCCESS) {
		throw VulkanException(res, "vkCreateSwapchainKHR", loc);
	}

	return handle;
}

void Device::vkDestroyImageView(VkImageView view) noexcept
{
	m_dt.vkDestroyImageView(m_handle, view, nullptr);
}

void Device::vkDestroySemaphore(VkSemaphore semaphore) noexcept
{
	m_dt.vkDestroySemaphore(m_handle, semaphore, nullptr);
}

void Device::vkDestroySwapchain(VkSwapchainKHR swapchain) noexcept
{
	m_dt.vkDestroySwapchainKHR(m_handle, swapchain, nullptr);
}

} // namespace voxen::gfx::vk
