#pragma once

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/window.hpp>

namespace voxen::client
{

class VulkanSwapchain {
public:
	explicit VulkanSwapchain(VulkanBackend &backend, Window &window);
	VulkanSwapchain(VulkanSwapchain &&) = delete;
	VulkanSwapchain(const VulkanSwapchain &) = delete;
	VulkanSwapchain &operator = (VulkanSwapchain &&) = delete;
	VulkanSwapchain &operator = (const VulkanSwapchain &) = delete;
	~VulkanSwapchain() noexcept;

	void recreateSwapchain();

	VkSurfaceKHR surfaceHandle() const noexcept { return m_surface; }
	VkSwapchainKHR swapchainHandle() const noexcept { return m_swapchain; }
	uint32_t numSwapchainImages() const noexcept { return uint32_t(m_swapchain_images.size()); }
	VkImage swapchainImage(uint32_t idx) const noexcept { return m_swapchain_images[idx]; }
	VkSurfaceFormatKHR surfaceFormat() const noexcept { return m_surface_format; }
	VkPresentModeKHR presentMode() const noexcept { return m_present_mode; }

	operator VkSwapchainKHR() const noexcept { return m_swapchain; }
private:
	VulkanBackend &m_backend;
	Window &m_window;
	VkSurfaceKHR m_surface = VK_NULL_HANDLE;
	VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
	std::vector<VkImage> m_swapchain_images;
	VkSurfaceFormatKHR m_surface_format;
	VkPresentModeKHR m_present_mode;

	void createSurface();
	void destroySurface() noexcept;
	void pickSurfaceFormat();
	void pickPresentMode();
	void destroySwapchain() noexcept;
};

}
