#pragma once

#include <voxen/client/vulkan/common.hpp>
#include <voxen/client/vulkan/memory.hpp>

namespace voxen::client::vulkan
{

class Buffer {
public:
	enum class Usage {
		DeviceLocal,
		Staging,
		Readback
	};

	Buffer(const VkBufferCreateInfo &info, Usage usage);
	Buffer(Buffer &&) noexcept;
	Buffer(const Buffer &) = delete;
	Buffer &operator = (Buffer &&) noexcept;
	Buffer &operator = (const Buffer &) = delete;
	~Buffer() noexcept;

	DeviceAllocation &allocation() noexcept { return m_memory; }

	VkBuffer handle() const noexcept { return m_buffer; }
	const DeviceAllocation &allocation() const noexcept { return m_memory; }
	VkDeviceSize size() const noexcept { return m_size; }

	operator VkBuffer() const noexcept { return m_buffer; }

private:
	VkBuffer m_buffer = VK_NULL_HANDLE;
	DeviceAllocation m_memory;
	VkDeviceSize m_size = 0;
};

}
