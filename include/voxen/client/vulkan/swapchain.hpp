#pragma once

#include <voxen/client/vulkan/common.hpp>
#include <voxen/client/window.hpp>

namespace voxen::client::vulkan
{

class Swapchain {
public:
	Swapchain();
	Swapchain(Swapchain &&) = delete;
	Swapchain(const Swapchain &) = delete;
	Swapchain &operator = (Swapchain &&) = delete;
	Swapchain &operator = (const Swapchain &) = delete;
	~Swapchain() noexcept;

	void recreateSwapchain();
	uint32_t acquireImage(VkSemaphore signal_semaphore);
	void presentImage(uint32_t idx, VkSemaphore wait_semaphore);

	uint32_t numImages() const noexcept;
	VkImage image(uint32_t idx) const noexcept { return m_images[idx]; }
	VkImageView imageView(uint32_t idx) const noexcept { return m_image_views[idx]; }
	VkExtent2D imageExtent() const noexcept { return m_image_extent; }

	operator VkSwapchainKHR() const noexcept { return m_swapchain; }
private:
	VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
	std::vector<VkImage> m_images;
	std::vector<VkImageView> m_image_views;
	VkExtent2D m_image_extent;

	void destroySwapchain() noexcept;
	VkExtent2D pickImageExtent(const VkSurfaceCapabilitiesKHR &caps);
	uint32_t pickImagesNumber(const VkSurfaceCapabilitiesKHR &caps);
	void obtainImages();
	void createImageViews();
};

}
