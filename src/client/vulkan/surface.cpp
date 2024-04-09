#include <voxen/client/vulkan/surface.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>
#include <voxen/client/vulkan/instance.hpp>
#include <voxen/client/vulkan/physical_device.hpp>
#include <voxen/util/error_condition.hpp>
#include <voxen/util/log.hpp>

#include <extras/defer.hpp>

#include <GLFW/glfw3.h>

namespace voxen::client::vulkan
{

Surface::Surface(Window &window) : m_window(window)
{
	Log::debug("Creating Surface");
	auto &backend = Backend::backend();
	VkInstance instance = backend.instance();
	auto allocator = HostAllocator::callbacks();

	VkResult result = glfwCreateWindowSurface(instance, window.glfwHandle(), allocator, &m_surface);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "glfwCreateWindowSurface");
	}
	defer_fail { backend.vkDestroySurfaceKHR(instance, m_surface, allocator); };

	checkPresentSupport();
	pickSurfaceFormat();
	pickPresentMode();
	Log::debug("Surface created successfully");
}

Surface::~Surface() noexcept
{
	Log::debug("Destroying Surface");
	auto &backend = Backend::backend();
	VkInstance instance = backend.instance();
	backend.vkDestroySurfaceKHR(instance, m_surface, HostAllocator::callbacks());
}

void Surface::checkPresentSupport()
{
	auto &backend = Backend::backend();
	// Present support should've been checked in VulkanQueueManager when
	// looking for a present queue family, so it's maybe a double check
	VkPhysicalDevice device = backend.physicalDevice();
	uint32_t family = backend.physicalDevice().presentQueueFamily();
	VkBool32 supported;
	VkResult result = backend.vkGetPhysicalDeviceSurfaceSupportKHR(device, family, m_surface, &supported);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkGetPhysicalDeviceSurfaceSupportKHR");
	}

	if (!supported) {
		Log::error("Selected GPU can't present to this window surface");
		Log::error("Earlier checks passed - most probably it's a bug in Voxen or GLFW");
		throw Exception::fromError(VoxenErrc::GfxCapabilityMissing, "Vulkan device can't present to window");
	}
}

void Surface::pickSurfaceFormat()
{
	auto &backend = Backend::backend();
	VkPhysicalDevice device = backend.physicalDevice();

	uint32_t num_formats;
	std::vector<VkSurfaceFormatKHR> formats;
	// Get number of surface formats
	VkResult result = backend.vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &num_formats, nullptr);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkGetPhysicalDeviceSurfaceFormatsKHR");
	}
	// Get descriptions of surface formats
	formats.resize(num_formats);
	result = backend.vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &num_formats, formats.data());
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkGetPhysicalDeviceSurfaceFormatsKHR");
	}

	for (auto format : formats) {
		if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			m_surface_format = format;
			return;
		}
	}
	Log::error("Surface format BGRA8_SRGB not found");
	throw Exception::fromError(VoxenErrc::GfxCapabilityMissing, "failed to find suitable surface format");
}

void Surface::pickPresentMode()
{
	auto &backend = Backend::backend();
	VkPhysicalDevice device = backend.physicalDevice();

	uint32_t num_modes;
	std::vector<VkPresentModeKHR> modes;
	// Get number of present modes
	VkResult result = backend.vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &num_modes, nullptr);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkGetPhysicalDeviceSurfacePresentModesKHR");
	}
	// Get descriptions of present modes
	modes.resize(num_modes);
	result = backend.vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &num_modes, modes.data());
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkGetPhysicalDeviceSurfacePresentModesKHR");
	}

	// TODO: support configurable/runtime-changeable present modes?
	for (auto mode : modes) {
		if (mode == VK_PRESENT_MODE_FIFO_KHR) {
			m_present_mode = mode;
			return;
		}
	}
	Log::error("Present mode VK_PRESENT_MODE_FIFO_KHR not found");
	throw Exception::fromError(VoxenErrc::GfxCapabilityMissing, "failed to find suitable present mode");
}

} // namespace voxen::client::vulkan
