#include <voxen/client/vulkan/swapchain.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>
#include <voxen/client/vulkan/instance.hpp>
#include <voxen/client/vulkan/physical_device.hpp>
#include <voxen/client/vulkan/surface.hpp>

#include <voxen/util/log.hpp>

#include <extras/defer.hpp>

namespace voxen::client::vulkan
{

Swapchain::Swapchain()
{
	Log::debug("Creating Swapchain");
	recreateSwapchain();
	Log::debug("Swapchain created successfully");
}

Swapchain::~Swapchain() noexcept
{
	Log::debug("Destroying Swapchain");
	destroySwapchain();
}

void Swapchain::recreateSwapchain()
{
	auto &backend = Backend::backend();
	assert(backend.surface() != nullptr);
	VkPhysicalDevice phys_device = backend.physicalDevice();
	VkDevice device = backend.device();
	VkSurfaceKHR surface = backend.surface();
	auto allocator = HostAllocator::callbacks();

	VkSwapchainCreateInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	info.surface = surface;
	info.imageArrayLayers = 1;
	// TODO: is that all?
	info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.queueFamilyIndexCount = 0;
	info.pQueueFamilyIndices = nullptr;
	info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	info.clipped = VK_TRUE;
	info.oldSwapchain = m_swapchain;

	{
		VkSurfaceFormatKHR surface_format = backend.surface().format();
		info.imageFormat = surface_format.format;
		info.imageColorSpace = surface_format.colorSpace;
		info.presentMode = backend.surface().presentMode();
	}

	{
		VkSurfaceCapabilitiesKHR caps;
		VkResult result = backend.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_device, surface, &caps);
		if (result != VK_SUCCESS)
			throw VulkanException(result, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

		info.imageExtent = pickImageExtent(caps);
		info.minImageCount = pickImagesNumber(caps);
		info.preTransform = caps.currentTransform;
	}

	VkSwapchainKHR new_swapchain;
	VkResult result = backend.vkCreateSwapchainKHR(device, &info, allocator, &new_swapchain);
	// Old swapchain is retired and should be destroyed in any case
	destroySwapchain();
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkCreateSwapchainKHR");

	m_swapchain = new_swapchain;
	defer_fail { destroySwapchain(); };

	m_image_extent = info.imageExtent;
	obtainImages();
	createImageViews();
}

uint32_t Swapchain::acquireImage(VkSemaphore signal_semaphore)
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device();

	uint32_t image_index;
	VkResult result = backend.vkAcquireNextImageKHR(device, m_swapchain, UINT64_MAX,
	                                                signal_semaphore, VK_NULL_HANDLE, &image_index);
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkAcquireNextImageKHR");
	return image_index;
}

void Swapchain::presentImage(uint32_t idx, VkSemaphore wait_semaphore)
{
	assert(idx < m_images.size());

	auto &backend = Backend::backend();
	VkQueue queue = backend.device().presentQueue();

	VkResult present_result;
	VkPresentInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	info.waitSemaphoreCount = wait_semaphore == VK_NULL_HANDLE ? 0 : 1;
	info.pWaitSemaphores = &wait_semaphore;
	info.swapchainCount = 1;
	info.pSwapchains = &m_swapchain;
	info.pImageIndices = &idx;
	info.pResults = &present_result;

	VkResult queue_result = backend.vkQueuePresentKHR(queue, &info);
	if (queue_result != VK_SUCCESS)
		throw VulkanException(queue_result, "vkQueuePresentKHR");
	if (present_result != VK_SUCCESS)
		throw VulkanException(present_result, "vkQueuePresentKHR[pResults]");
}

uint32_t Swapchain::numImages() const noexcept
{
	// We guarantee that `m_images.size()` will always fit in `uint32_t` because
	// Vulkan uses this type in `vkGetSwapchainImagesKHR` for enumerating images
	return uint32_t(m_images.size());
}

void Swapchain::destroySwapchain() noexcept
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device();
	auto allocator = HostAllocator::callbacks();

	m_image_extent = { 0, 0 };
	for (VkImageView view : m_image_views)
		backend.vkDestroyImageView(device, view, allocator);
	m_image_views.clear();
	// Images are destroyed automatically with swapchain
	m_images.clear();

	backend.vkDestroySwapchainKHR(device, m_swapchain, allocator);
	m_swapchain = VK_NULL_HANDLE;
}

VkExtent2D Swapchain::pickImageExtent(const VkSurfaceCapabilitiesKHR &caps)
{
	VkExtent2D result;
	if (caps.currentExtent.width == 0 && caps.currentExtent.height == 0) {
		// TODO: should it be merged with the second case instead?
		Log::error("Current surface extent is (0, 0), can't create swapchain now (window is minimized?)");
		throw MessageException("can't create swapchain now");
	} else if (caps.currentExtent.width == UINT32_MAX && caps.currentExtent.height == UINT32_MAX) {
		Log::debug("Current surface extent is undefined, using GLFW window size");
		auto[width, height] = Backend::backend().surface().window().framebufferSize();
		result.width = std::clamp(uint32_t(width), caps.minImageExtent.width, caps.maxImageExtent.width);
		result.height = std::clamp(uint32_t(height), caps.minImageExtent.height, caps.maxImageExtent.height);
	} else {
		result = caps.currentExtent;
	}
	Log::info("Requesting {}x{} swapchain images", result.width, result.height);
	return result;
}

uint32_t Swapchain::pickImagesNumber(const VkSurfaceCapabilitiesKHR &caps)
{
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
	return num_images;
}

void Swapchain::obtainImages()
{
	assert(m_swapchain != VK_NULL_HANDLE && m_images.empty());

	auto &backend = Backend::backend();
	VkDevice device = backend.device();

	uint32_t num_images;
	VkResult result = backend.vkGetSwapchainImagesKHR(device, m_swapchain, &num_images, nullptr);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkGetSwapchainImagesKHR");
	}

	Log::info("Swapchain has {} images", num_images);
	m_images.resize(num_images);
	result = backend.vkGetSwapchainImagesKHR(device, m_swapchain, &num_images, m_images.data());
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkGetSwapchainImagesKHR");
	}
}

void Swapchain::createImageViews()
{
	assert(m_swapchain != VK_NULL_HANDLE && m_image_views.empty());

	auto &backend = Backend::backend();
	VkDevice device = backend.device();
	auto allocator = HostAllocator::callbacks();

	VkImageViewCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	info.format = backend.surface().format().format;
	info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	info.subresourceRange.baseMipLevel = 0;
	info.subresourceRange.levelCount = 1;
	info.subresourceRange.baseArrayLayer = 0;
	info.subresourceRange.layerCount = 1;

	// Position of failed image
	size_t failed_index = 0;
	VkResult error_code = VK_SUCCESS;
	m_image_views.resize(m_images.size());
	for (size_t i = 0; i < m_images.size(); i++) {
		info.image = m_images[i];
		VkResult result = backend.vkCreateImageView(device, &info, allocator, &m_image_views[i]);
		if (result != VK_SUCCESS) {
			failed_index = i;
			error_code = result;
			break;
		}
	}

	if (error_code == VK_SUCCESS) {
		return;
	}
	// Destroy successfully created images
	for (size_t i = 0; i < failed_index; i++) {
		backend.vkDestroyImageView(device, m_image_views[i], allocator);
	}
	throw VulkanException(error_code, "vkCreateImageView");
}

}
