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
	Buffer(Buffer &&) = delete;
	Buffer(const Buffer &) = delete;
	Buffer &operator = (Buffer &&) = delete;
	Buffer &operator = (const Buffer &) = delete;
	~Buffer() noexcept;

	DeviceAllocation &allocation() { return *m_memory; }

	operator VkBuffer() const noexcept { return m_buffer; }
private:
	VkBuffer m_buffer = VK_NULL_HANDLE;
	std::shared_ptr<DeviceAllocation> m_memory;
};

}
