#pragma once

#include <voxen/client/vulkan/common.hpp>

#include <voxen/client/window.hpp>

namespace voxen::client
{

class VulkanSurface {
public:
	explicit VulkanSurface(Window &window);
	VulkanSurface(VulkanSurface &&) = delete;
	VulkanSurface(const VulkanSurface &) = delete;
	VulkanSurface &operator = (VulkanSurface &&) = delete;
	VulkanSurface &operator = (const VulkanSurface &) = delete;
	~VulkanSurface() noexcept;

	Window &window() noexcept { return m_window; }
	VkSurfaceFormatKHR format() const noexcept { return m_surface_format; }
	VkPresentModeKHR presentMode() const noexcept { return m_present_mode; }

	operator VkSurfaceKHR() const noexcept { return m_surface; }
private:
	VkSurfaceKHR m_surface = VK_NULL_HANDLE;
	Window &m_window;
	VkSurfaceFormatKHR m_surface_format;
	VkPresentModeKHR m_present_mode;

	void checkPresentSupport();
	void pickSurfaceFormat();
	void pickPresentMode();
};

}
