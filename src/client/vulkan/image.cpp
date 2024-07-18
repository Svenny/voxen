#include <voxen/client/vulkan/image.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/gfx/vk/vk_device.hpp>

#include <extras/defer.hpp>

#include <vma/vk_mem_alloc.h>

namespace voxen::client::vulkan
{

Image::Image(const VkImageCreateInfo &info)
{
	auto &backend = Backend::backend();

	// TODO: For now we support only device-local images
	VmaAllocationCreateInfo alloc_info {};
	alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	VkResult res = vmaCreateImage(backend.device().vma(), &info, &alloc_info, &m_image, &m_alloc, nullptr);
	if (res != VK_SUCCESS) {
		throw VulkanException(res, "vmaCreateImage");
	}
}

Image::~Image() noexcept
{
	auto &backend = Backend::backend();
	backend.device().enqueueDestroy(m_image, m_alloc);
}

} // namespace voxen::client::vulkan
