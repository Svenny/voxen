#pragma once

#include <voxen/client/vulkan/common.hpp>

#include <voxen/client/window.hpp>

namespace voxen::client::vulkan
{

class Surface {
public:
	explicit Surface(Window &window);
	Surface(Surface &&) = delete;
	Surface(const Surface &) = delete;
	Surface &operator=(Surface &&) = delete;
	Surface &operator=(const Surface &) = delete;
	~Surface() noexcept;

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

} // namespace voxen::client::vulkan
