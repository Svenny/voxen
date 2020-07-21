#pragma once

#include <voxen/client/vulkan/common.hpp>
#include <voxen/client/vulkan/memory.hpp>

namespace voxen::client
{

class VulkanBuffer {
public:
	enum class Usage {
		DeviceLocal,
		Staging,
		Readback
	};

	VulkanBuffer(const VkBufferCreateInfo &info, Usage usage);
	VulkanBuffer(VulkanBuffer &&) = delete;
	VulkanBuffer(const VulkanBuffer &) = delete;
	VulkanBuffer &operator = (VulkanBuffer &&) = delete;
	VulkanBuffer &operator = (const VulkanBuffer &) = delete;
	~VulkanBuffer() noexcept;

	operator VkBuffer() const noexcept { return m_buffer; }
private:
	VkBuffer m_buffer;
	VulkanDeviceAllocation m_memory;
};

}
