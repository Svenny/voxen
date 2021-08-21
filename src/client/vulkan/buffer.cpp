#include <voxen/client/vulkan/buffer.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>

#include <extras/defer.hpp>

namespace voxen::client::vulkan
{

WrappedVkBuffer::WrappedVkBuffer(const VkBufferCreateInfo &info)
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device();
	VkResult result = backend.vkCreateBuffer(device, &info, HostAllocator::callbacks(), &m_handle);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkCreateBuffer");
	}
}

WrappedVkBuffer::WrappedVkBuffer(WrappedVkBuffer &&other) noexcept
{
	m_handle = std::exchange(other.m_handle, static_cast<VkBuffer>(VK_NULL_HANDLE));
}

WrappedVkBuffer &WrappedVkBuffer::operator = (WrappedVkBuffer &&other) noexcept
{
	std::swap(m_handle, other.m_handle);
	return *this;
}

WrappedVkBuffer::~WrappedVkBuffer() noexcept
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device();
	backend.vkDestroyBuffer(device, m_handle, HostAllocator::callbacks());
}

void WrappedVkBuffer::bindMemory(VkDeviceMemory memory, VkDeviceSize offset)
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device();
	VkResult result = backend.vkBindBufferMemory(device, m_handle, memory, offset);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkBindBufferMemory");
	}
}

void WrappedVkBuffer::getMemoryRequirements(VkMemoryRequirements &reqs) const noexcept
{
	auto &backend = Backend::backend();
	backend.vkGetBufferMemoryRequirements(backend.device(), m_handle, &reqs);
}

FatVkBuffer::FatVkBuffer(const VkBufferCreateInfo &info, DeviceMemoryUseCase use_case)
	: m_buffer(info), m_size(info.size)
{
	auto &backend = Backend::backend();
	m_memory = backend.deviceAllocator().allocate(m_buffer, use_case);
	m_buffer.bindMemory(m_memory.handle(), m_memory.offset());
}

FatVkBuffer::FatVkBuffer(FatVkBuffer &&other) noexcept
{
	m_buffer = std::move(other.m_buffer);
	m_memory = std::move(other.m_memory);
	m_size = std::exchange(other.m_size, 0);
}

FatVkBuffer &FatVkBuffer::operator = (FatVkBuffer &&other) noexcept
{
	std::swap(m_buffer, other.m_buffer);
	std::swap(m_memory, other.m_memory);
	std::swap(m_size, other.m_size);
	return *this;
}

}
