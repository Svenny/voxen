#pragma once

#include <voxen/client/vulkan/common.hpp>
#include <voxen/gfx/vk/vma_fwd.hpp>

namespace voxen::client::vulkan
{

// This is a "fat", "fully self-managed" wrapper around `VkBuffer` handle.
// It manages memory allocation automatically and is generally universal.
class FatVkBuffer {
public:
	enum class Usage {
		DeviceLocal,
		Staging,
		Readback
	};

	FatVkBuffer() = default;
	explicit FatVkBuffer(const VkBufferCreateInfo &info, Usage usage);
	FatVkBuffer(FatVkBuffer &&) noexcept;
	FatVkBuffer(const FatVkBuffer &) = delete;
	FatVkBuffer &operator=(FatVkBuffer &&) noexcept;
	FatVkBuffer &operator=(const FatVkBuffer &) = delete;
	~FatVkBuffer() noexcept;

	VkBuffer handle() const noexcept { return m_handle; }
	VkDeviceSize size() const noexcept { return m_size; }
	// Null for `DeviceLocal` usage, non-null for `Staging` and `Readback`
	void *hostPointer() const noexcept { return m_host_pointer; }

	operator VkBuffer() const noexcept { return m_handle; }

private:
	VkBuffer m_handle = VK_NULL_HANDLE;
	VmaAllocation m_memory = VK_NULL_HANDLE;
	VkDeviceSize m_size = 0;
	void *m_host_pointer = nullptr;
};

} // namespace voxen::client::vulkan
