#include <voxen/client/vulkan/image_view.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>

namespace voxen::client
{

VulkanImageView::VulkanImageView(const VkImageViewCreateInfo &info) {
	auto &backend = VulkanBackend::backend();
	VkDevice device = *backend.device();
	VkResult result = backend.vkCreateImageView(device, &info, VulkanHostAllocator::callbacks(), &m_view);
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkCreateImageView");
}

VulkanImageView::~VulkanImageView() noexcept {
	auto &backend = VulkanBackend::backend();
	VkDevice device = *backend.device();
	backend.vkDestroyImageView(device, m_view, VulkanHostAllocator::callbacks());
}

}
