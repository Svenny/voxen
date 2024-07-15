#include <voxen/gfx/vk/vk_instance.hpp>

#include <voxen/client/vulkan/common.hpp>

namespace voxen::gfx::vk
{

// TODO: there parts are not yet moved to voxen/gfx/vk
using client::vulkan::VulkanException;

void Instance::vkDestroySurface(VkSurfaceKHR surface) noexcept
{
	m_dt.vkDestroySurfaceKHR(m_handle, surface, nullptr);
}

VkSurfaceCapabilitiesKHR Instance::vkGetPhysicalDeviceSurfaceCapabilities(VkPhysicalDevice physical_device,
	VkSurfaceKHR surface, SLoc loc)
{
	VkSurfaceCapabilitiesKHR caps {};
	VkResult res = m_dt.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &caps);
	if (res != VK_SUCCESS) {
		throw VulkanException(res, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR", loc);
	}

	return caps;
}

extras::dyn_array<VkSurfaceFormatKHR> Instance::vkGetPhysicalDeviceSurfaceFormats(VkPhysicalDevice physical_device,
	VkSurfaceKHR surface, SLoc loc)
{
	uint32_t count = 0;
	VkResult res = m_dt.vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &count, nullptr);
	if (res != VK_SUCCESS) {
		throw VulkanException(res, "vkGetPhysicalDeviceSurfaceFormatsKHR", loc);
	}

	extras::dyn_array<VkSurfaceFormatKHR> formats(count);
	res = m_dt.vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &count, formats.data());
	if (res != VK_SUCCESS) {
		throw VulkanException(res, "vkGetPhysicalDeviceSurfaceFormatsKHR", loc);
	}

	return formats;
}

extras::dyn_array<VkPresentModeKHR> Instance::vkGetPhysicalDeviceSurfacePresentModes(VkPhysicalDevice physical_device,
	VkSurfaceKHR surface, SLoc loc)
{
	uint32_t count = 0;
	VkResult res = m_dt.vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &count, nullptr);
	if (res != VK_SUCCESS) {
		throw VulkanException(res, "vkGetPhysicalDeviceSurfacePresentModesKHR", loc);
	}

	extras::dyn_array<VkPresentModeKHR> modes(count);
	res = m_dt.vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &count, modes.data());
	if (res != VK_SUCCESS) {
		throw VulkanException(res, "vkGetPhysicalDeviceSurfacePresentModesKHR", loc);
	}

	return modes;
}

} // namespace voxen::gfx::vk
