#include <voxen/client/vulkan/image_view.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/gfx/vk/vk_device.hpp>

namespace voxen::client::vulkan
{

ImageView::ImageView(const VkImageViewCreateInfo &info)
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device().handle();
	VkResult result = backend.vkCreateImageView(device, &info, HostAllocator::callbacks(), &m_view);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkCreateImageView");
	}
}

ImageView::~ImageView() noexcept
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device().handle();
	backend.vkDestroyImageView(device, m_view, HostAllocator::callbacks());
}

} // namespace voxen::client::vulkan
