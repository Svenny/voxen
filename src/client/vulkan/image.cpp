#include <voxen/client/vulkan/image.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>

#include <extras/defer.hpp>

namespace voxen::client::vulkan
{

Image::Image(const VkImageCreateInfo &info)
{
	auto &backend = Backend::backend();
	VkDevice device = *backend.device();
	auto allocator = VulkanHostAllocator::callbacks();

	VkResult result = backend.vkCreateImage(device, &info, allocator, &m_image);
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkCreateImage");
	defer_fail { backend.vkDestroyImage(device, m_image, allocator); };

	DeviceAllocationRequirements reqs = {};
	backend.vkGetImageMemoryRequirements(device, m_image, &reqs.memory_reqs);
	// For now we support only device-local images
	reqs.need_host_visibility = false;
	reqs.prefer_host_coherence = false;
	reqs.prefer_host_caching = false;
	m_memory = backend.deviceAllocator()->allocate(reqs);

	result = backend.vkBindImageMemory(device, m_image, m_memory->handle(), m_memory->offset());
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkBindImageMemory");
}

Image::~Image() noexcept
{
	auto &backend = Backend::backend();
	VkDevice device = *backend.device();
	backend.vkDestroyImage(device, m_image, VulkanHostAllocator::callbacks());
}

}
