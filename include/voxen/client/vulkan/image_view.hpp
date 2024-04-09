#pragma once

#include <voxen/client/vulkan/command_buffer.hpp>

namespace voxen::client::vulkan
{

class ImageView {
public:
	ImageView(const VkImageViewCreateInfo &info);
	ImageView(ImageView &&) = delete;
	ImageView(const ImageView &) = delete;
	ImageView &operator=(ImageView &&) = delete;
	ImageView &operator=(const ImageView &) = delete;
	~ImageView() noexcept;

	operator VkImageView() const noexcept { return m_view; }

private:
	VkImageView m_view;
};

} // namespace voxen::client::vulkan
