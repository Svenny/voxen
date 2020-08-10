#pragma once

#include <voxen/client/vulkan/common.hpp>
#include <voxen/client/vulkan/memory.hpp>

namespace voxen::client::vulkan
{

class Image {
public:
	Image(const VkImageCreateInfo &info);
	Image(Image &&) = delete;
	Image(const Image &) = delete;
	Image &operator = (Image &&) = delete;
	Image &operator = (const Image &) = delete;
	~Image() noexcept;

	DeviceAllocation &allocation() { return *m_memory; }

	operator VkImage() const noexcept { return m_image; }
private:
	VkImage m_image = VK_NULL_HANDLE;
	std::shared_ptr<DeviceAllocation> m_memory;
};

}
