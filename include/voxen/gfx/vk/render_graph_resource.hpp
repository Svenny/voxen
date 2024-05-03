#pragma once

#include <voxen/visibility.hpp>

#include <vulkan/vulkan.h>

namespace voxen::gfx::vk
{

// References a buffer resource created by `RenderGraphBuilder`
class VOXEN_API RenderGraphBuffer {
public:
	struct Private;

	RenderGraphBuffer() noexcept = default;
	RenderGraphBuffer(Private &priv) noexcept;
	RenderGraphBuffer(RenderGraphBuffer &&) noexcept;
	RenderGraphBuffer(const RenderGraphBuffer &) = delete;
	RenderGraphBuffer &operator=(RenderGraphBuffer &&) noexcept;
	RenderGraphBuffer &operator=(const RenderGraphBuffer &) = delete;
	~RenderGraphBuffer() noexcept;

	// Returned handle is valid only during one render graph execution
	VkBuffer handle() const noexcept { return m_handle; }

	// This is an internal method
	void setHandle(VkBuffer handle) noexcept { m_handle = handle; }
	// This is an internal method
	Private *getPrivate() const noexcept { return m_private; }

protected:
	Private *m_private = nullptr;

	VkBuffer m_handle = VK_NULL_HANDLE;
};

// References an image resource created by `RenderGraphBuilder`
class VOXEN_API RenderGraphImage {
public:
	struct Private;

	RenderGraphImage() noexcept = default;
	RenderGraphImage(Private &priv) noexcept;
	RenderGraphImage(RenderGraphImage &&) noexcept;
	RenderGraphImage(const RenderGraphImage &) = delete;
	RenderGraphImage &operator=(RenderGraphImage &&) noexcept;
	RenderGraphImage &operator=(const RenderGraphImage &) = delete;
	~RenderGraphImage() noexcept;

	// Returned handle is valid only during one render graph execution
	VkImage handle() const noexcept { return m_handle; }

	// This is an internal method
	void setHandle(VkImage handle) noexcept { m_handle = handle; }
	// This is an internal method
	Private *getPrivate() const noexcept { return m_private; }

protected:
	Private *m_private = nullptr;

	VkImage m_handle = VK_NULL_HANDLE;
};

// References an image view created by `RenderGraphBuilder`
class VOXEN_API RenderGraphImageView {
public:
	struct Private;

	RenderGraphImageView() noexcept = default;
	RenderGraphImageView(Private &priv) noexcept;
	RenderGraphImageView(RenderGraphImageView &&) noexcept;
	RenderGraphImageView(const RenderGraphImageView &) = delete;
	RenderGraphImageView &operator=(RenderGraphImageView &&) noexcept;
	RenderGraphImageView &operator=(const RenderGraphImageView &) = delete;
	~RenderGraphImageView() noexcept;

	// Returned handle is valid only during one render graph execution
	VkImageView handle() const noexcept { return m_handle; }

	// This is an internal method
	void setHandle(VkImageView handle) noexcept { m_handle = handle; }
	// This is an internal method
	Private *getPrivate() const noexcept { return m_private; }

protected:
	Private *m_private = nullptr;

	VkImageView m_handle = VK_NULL_HANDLE;
};

} // namespace voxen::gfx::vk
