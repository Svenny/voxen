#include <voxen/client/vulkan/swapchain.hpp>

#include <voxen/client/vulkan/instance.hpp>
#include <voxen/client/vulkan/device.hpp>
#include	<voxen/util/log.hpp>

#include <extras/defer.hpp>

#include <GLFW/glfw3.h>

namespace voxen::client
{

VulkanSwapchain::VulkanSwapchain(VulkanBackend &backend, Window &window)
	: m_backend(backend), m_window(window)
{
	createSurface();
	defer_fail { destroySurface(); };
	recreateSwapchain();
	Log::debug("VulkanSwapchain created successfully");
}

VulkanSwapchain::~VulkanSwapchain()
{
	Log::debug("Destroying VulkanSwapchain");
	destroySwapchain();
	destroySurface();
}

void VulkanSwapchain::recreateSwapchain()
{
	VkPhysicalDevice phys_device = m_backend.device()->physDeviceHandle();
	VkDevice device = m_backend.device()->deviceHandle();
	auto allocator = VulkanHostAllocator::callbacks();

	VkSurfaceCapabilitiesKHR caps;
	VkResult result = m_backend.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_device, m_surface, &caps);
	if (result != VK_SUCCESS)
		throw VulkanException(result);

	// Check for surface extent validity
	if (caps.currentExtent.width == 0 && caps.currentExtent.height == 0) {
		Log::error("Current surface extent is (0, 0), can't create swapchain now (window is minimized?)");
		throw MessageException("can't create swapchain now");
	} else if (caps.currentExtent.width == UINT32_MAX && caps.currentExtent.height == UINT32_MAX) {
		Log::debug("Current surface extent is undefined, using GLFW window size");
		auto[width, height] = m_window.framebufferSize();
		caps.currentExtent.width = std::clamp(uint32_t(width), caps.minImageExtent.width, caps.maxImageExtent.width);
		caps.currentExtent.height = std::clamp(uint32_t(height), caps.minImageExtent.height, caps.maxImageExtent.height);
	}
	Log::info("Requesting {}x{} swapchain images", caps.currentExtent.width, caps.currentExtent.height);

	// Select the number of swapchain images
	// TODO: support selecting 2/3?
	uint32_t num_images = 3;
	if (num_images < caps.minImageCount) {
		Log::warn("Surface supports at least {} images, but we want {}", caps.minImageCount, num_images);
		num_images = caps.minImageCount;
	} else if (caps.maxImageCount != 0 && num_images > caps.maxImageCount) {
		Log::warn("Surface supports up to {} images, but we want {}", caps.maxImageCount, num_images);
		num_images = caps.maxImageCount;
	}
	Log::debug("Requesting at least {} swapchain images", num_images);

	VkSurfaceFormatKHR surface_format = pickSurfaceFormat();
	VkPresentModeKHR present_mode = pickPresentMode();

	// Fill VkSwapchainCreateInfoKHR
	VkSwapchainCreateInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	info.surface = m_surface;
	info.minImageCount = num_images;
	info.imageFormat = surface_format.format;
	info.imageColorSpace = surface_format.colorSpace;
	info.imageExtent = caps.currentExtent;
	info.imageArrayLayers = 1;
	// TODO: is that all?
	info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.queueFamilyIndexCount = 0;
	info.pQueueFamilyIndices = nullptr;
	info.preTransform = caps.currentTransform;
	info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	info.presentMode = present_mode;
	info.clipped = VK_TRUE;
	info.oldSwapchain = m_swapchain;

	result = m_backend.vkCreateSwapchainKHR(device, &info, allocator, &m_swapchain);
	if (result != VK_SUCCESS)
		throw VulkanException(result);
	defer_fail { destroySwapchain(); };

	result = m_backend.vkGetSwapchainImagesKHR(device, m_swapchain, &num_images, nullptr);
	if (result != VK_SUCCESS)
		throw VulkanException(result);

	Log::info("Swapchain has {} images", num_images);
	m_swapchain_images.resize(num_images);
	result = m_backend.vkGetSwapchainImagesKHR(device, m_swapchain, &num_images, m_swapchain_images.data());
	if (result != VK_SUCCESS)
		throw VulkanException(result);
}

void VulkanSwapchain::createSurface()
{
	VkInstance instance = *m_backend.instance();
	GLFWwindow *window = m_window.glfwHandle();
	auto allocator = VulkanHostAllocator::callbacks();

	VkResult result = glfwCreateWindowSurface(instance, window, allocator, &m_surface);
	if (result != VK_SUCCESS)
		throw VulkanException(result);
}

void VulkanSwapchain::destroySurface()
{
	m_backend.vkDestroySurfaceKHR(*m_backend.instance(), m_surface, VulkanHostAllocator::callbacks());
}

VkSurfaceFormatKHR VulkanSwapchain::pickSurfaceFormat()
{
	VkPhysicalDevice device = m_backend.device()->physDeviceHandle();
	VkResult result;

	uint32_t num_formats;
	std::vector<VkSurfaceFormatKHR> formats;
	// Get number of surface formats
	result = m_backend.vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &num_formats, nullptr);
	if (result != VK_SUCCESS)
		throw VulkanException(result);
	// Get descriptions of surface formats
	formats.resize(num_formats);
	result = m_backend.vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &num_formats, formats.data());
	if (result != VK_SUCCESS)
		throw VulkanException(result);

	for (auto format : formats) {
		if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			return format;
	}
	Log::error("Surface format BGRA8_SRGB not found");
	throw MessageException("failed to find suitable surface format");
}

VkPresentModeKHR VulkanSwapchain::pickPresentMode()
{
	VkPhysicalDevice device = m_backend.device()->physDeviceHandle();
	VkResult result;

	uint32_t num_modes;
	std::vector<VkPresentModeKHR> modes;
	// Get number of present modes
	result = m_backend.vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &num_modes, nullptr);
	if (result != VK_SUCCESS)
		throw VulkanException(result);
	// Get description of present modes
	modes.resize(num_modes);
	result = m_backend.vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &num_modes, modes.data());
	if (result != VK_SUCCESS)
		throw VulkanException(result);

	// TODO: support configurable/runtime-changeable present modes?
	for (auto mode : modes) {
		if (mode == VK_PRESENT_MODE_FIFO_KHR)
			return mode;
	}
	Log::error("Present mode VK_PRESENT_MODE_FIFO_KHR not found");
	throw MessageException("failed to find suitable present mode");
}

void VulkanSwapchain::destroySwapchain()
{
	m_backend.vkDestroySwapchainKHR(m_backend.device()->deviceHandle(), m_swapchain, VulkanHostAllocator::callbacks());
	m_swapchain = VK_NULL_HANDLE;
	m_swapchain_images.clear();
}

}
