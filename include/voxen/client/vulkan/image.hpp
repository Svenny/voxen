#pragma once

#include <voxen/client/vulkan/common.hpp>
#include <voxen/gfx/vk/vma_fwd.hpp>

namespace voxen::client::vulkan
{

class Image {
public:
	Image(const VkImageCreateInfo &info);
	Image(Image &&) = delete;
	Image(const Image &) = delete;
	Image &operator=(Image &&) = delete;
	Image &operator=(const Image &) = delete;
	~Image() noexcept;

	VkImage handle() const noexcept { return m_image; }

	operator VkImage() const noexcept { return m_image; }

private:
	VkImage m_image = VK_NULL_HANDLE;
	VmaAllocation m_alloc = VK_NULL_HANDLE;
};

} // namespace voxen::client::vulkan
