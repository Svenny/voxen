#include <voxen/client/vulkan/buffer.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/gfx/vk/vk_device.hpp>

#include <extras/defer.hpp>

#include <vma/vk_mem_alloc.h>

namespace voxen::client::vulkan
{

FatVkBuffer::FatVkBuffer(const VkBufferCreateInfo &info, Usage usage) : m_size(info.size)
{
	VmaAllocator vma = Backend::backend().device().vma();

	VmaAllocationCreateInfo alloc_create_info {};
	switch (usage) {
	case Usage::DeviceLocal:
		alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		break;
	case Usage::Staging:
		alloc_create_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT
			| VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO;
		break;
	case Usage::Readback:
		alloc_create_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
		alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
		break;
	}

	VmaAllocationInfo vma_alloc_info {};

	VkResult res = vmaCreateBuffer(vma, &info, &alloc_create_info, &m_handle, &m_memory, &vma_alloc_info);
	if (res != VK_SUCCESS) {
		throw VulkanException(res, "vmaAllocateMemoryForBuffer");
	}

	m_host_pointer = vma_alloc_info.pMappedData;
}

FatVkBuffer::FatVkBuffer(FatVkBuffer &&other) noexcept
{
	m_handle = std::exchange(other.m_handle, VK_NULL_HANDLE);
	m_memory = std::exchange(other.m_memory, VK_NULL_HANDLE);
	m_size = std::exchange(other.m_size, 0);
	m_host_pointer = std::exchange(other.m_host_pointer, nullptr);
}

FatVkBuffer &FatVkBuffer::operator=(FatVkBuffer &&other) noexcept
{
	std::swap(m_handle, other.m_handle);
	std::swap(m_memory, other.m_memory);
	std::swap(m_size, other.m_size);
	std::swap(m_host_pointer, other.m_host_pointer);
	return *this;
}

FatVkBuffer::~FatVkBuffer() noexcept
{
	auto &device = Backend::backend().device();
	device.enqueueDestroy(m_handle, m_memory);
}

} // namespace voxen::client::vulkan
