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
	~VulkanSwapchain();

	void recreateSwapchain();

	VkSurfaceKHR surfaceHandle() const noexcept { return m_surface; }
	VkSwapchainKHR swapchainHandle() const noexcept { return m_swapchain; }
	uint32_t numSwapchainImages() const noexcept { return uint32_t(m_swapchain_images.size()); }
	VkImage swapchainImage(uint32_t idx) const noexcept { return m_swapchain_images[idx]; }

	operator VkSwapchainKHR() const noexcept { return m_swapchain; }
private:
	VulkanBackend &m_backend;
	Window &m_window;
	VkSurfaceKHR m_surface = VK_NULL_HANDLE;
	VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
	std::vector<VkImage> m_swapchain_images;

	void createSurface();
	void destroySurface();
	VkSurfaceFormatKHR pickSurfaceFormat();
	VkPresentModeKHR pickPresentMode();
	void destroySwapchain();
};

}
