#pragma once

#include <voxen/client/vulkan/common.hpp>
#include <voxen/client/vulkan/memory.hpp>

namespace voxen::client::vulkan
{

// This is a "dumb" wrapper around `VkBuffer` handle. It has only trivial logic and is designed
// to be as flexible as possible. It can be used either directly or via some higher-level class.
class WrappedVkBuffer final {
public:
	constexpr explicit WrappedVkBuffer(VkBuffer handle = VK_NULL_HANDLE) noexcept : m_handle(handle) {}
	explicit WrappedVkBuffer(const VkBufferCreateInfo &info);
	WrappedVkBuffer(WrappedVkBuffer &&other) noexcept;
	WrappedVkBuffer(const WrappedVkBuffer &) = delete;
	WrappedVkBuffer &operator=(WrappedVkBuffer &&other) noexcept;
	WrappedVkBuffer &operator=(const WrappedVkBuffer &) = delete;
	~WrappedVkBuffer() noexcept;

	// A dumb wrapper for `vkBindBufferMemory`
	void bindMemory(VkDeviceMemory memory, VkDeviceSize offset);
	// A dumb wrapper for `vkGetBufferMemoryRequirements`
	void getMemoryRequirements(VkMemoryRequirements &reqs) const noexcept;

	VkBuffer handle() const noexcept { return m_handle; }
	operator VkBuffer() const noexcept { return m_handle; }

private:
	VkBuffer m_handle = VK_NULL_HANDLE;
};

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
	explicit FatVkBuffer(const VkBufferCreateInfo &info, DeviceMemoryUseCase use_case);
	FatVkBuffer(FatVkBuffer &&) noexcept;
	FatVkBuffer(const FatVkBuffer &) = delete;
	FatVkBuffer &operator=(FatVkBuffer &&) noexcept;
	FatVkBuffer &operator=(const FatVkBuffer &) = delete;
	~FatVkBuffer() = default;

	DeviceAllocation &allocation() noexcept { return m_memory; }

	VkBuffer handle() const noexcept { return m_buffer.handle(); }
	const DeviceAllocation &allocation() const noexcept { return m_memory; }
	VkDeviceSize size() const noexcept { return m_size; }

	operator VkBuffer() const noexcept { return m_buffer.handle(); }

private:
	WrappedVkBuffer m_buffer;
	DeviceAllocation m_memory;
	VkDeviceSize m_size = 0;
};

} // namespace voxen::client::vulkan
