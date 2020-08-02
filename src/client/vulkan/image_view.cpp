#include <voxen/client/vulkan/image_view.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>

namespace voxen::client::vulkan
{

ImageView::ImageView(const VkImageViewCreateInfo &info)
{
	auto &backend = Backend::backend();
	VkDevice device = *backend.device();
	VkResult result = backend.vkCreateImageView(device, &info, VulkanHostAllocator::callbacks(), &m_view);
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkCreateImageView");
}

ImageView::~ImageView() noexcept
{
	auto &backend = Backend::backend();
	VkDevice device = *backend.device();
	backend.vkDestroyImageView(device, m_view, VulkanHostAllocator::callbacks());
}

}
