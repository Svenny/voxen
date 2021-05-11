#include <voxen/client/vulkan/buffer.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>

#include <extras/defer.hpp>

namespace voxen::client::vulkan
{

Buffer::Buffer(const VkBufferCreateInfo &info, Usage usage)
	: m_size(info.size)
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device();
	auto allocator = VulkanHostAllocator::callbacks();

	VkResult result = backend.vkCreateBuffer(device, &info, allocator, &m_buffer);
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkCreateBuffer");
	defer_fail { backend.vkDestroyBuffer(device, m_buffer, allocator); };

	DeviceAllocator::ResourceAllocationInfo reqs {
		.dedicated_if_preferred = false,
		.force_dedicated = false
	};
	// TODO: get rid of this enum in favor of `DeviceMemoryUseCase`
	switch (usage) {
	case Usage::DeviceLocal:
		reqs.use_case = DeviceMemoryUseCase::GpuOnly;
		break;
	case Usage::Staging:
		reqs.use_case = DeviceMemoryUseCase::Upload;
		break;
	case Usage::Readback:
		reqs.use_case = DeviceMemoryUseCase::Readback;
		break;
	}
	m_memory = backend.deviceAllocator().allocate(m_buffer, reqs);

	result = backend.vkBindBufferMemory(device, m_buffer, m_memory.handle(), m_memory.offset());
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkBindBufferMemory");
}

Buffer::Buffer(Buffer &&other) noexcept
{
	m_buffer = std::exchange(other.m_buffer, static_cast<VkBuffer>(VK_NULL_HANDLE));
	m_memory = std::move(other.m_memory);
	m_size = std::move(other.m_size);
}

Buffer &Buffer::operator = (Buffer &&other) noexcept
{
	m_buffer = std::exchange(other.m_buffer, static_cast<VkBuffer>(VK_NULL_HANDLE));
	m_memory = std::move(other.m_memory);
	m_size = std::move(other.m_size);
	return *this;
}

Buffer::~Buffer() noexcept
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device();
	backend.vkDestroyBuffer(device, m_buffer, VulkanHostAllocator::callbacks());
}

}
