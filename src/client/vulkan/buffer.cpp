#include <voxen/client/vulkan/buffer.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>

#include <extras/defer.hpp>

namespace voxen::client
{

VulkanBuffer::VulkanBuffer(const VkBufferCreateInfo &info)
{
	auto &backend = VulkanBackend::backend();
	VkDevice device = *backend.device();
	auto allocator = VulkanHostAllocator::callbacks();

	VkResult result = backend.vkCreateBuffer(device, &info, allocator, &m_buffer);
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkCreateBuffer");
	defer_fail { backend.vkDestroyBuffer(device, m_buffer, allocator); };

	VkMemoryRequirements mem_reqs;
	backend.vkGetBufferMemoryRequirements(device, m_buffer, &mem_reqs);
	m_memory = backend.deviceAllocator()->allocate(mem_reqs);

	result = backend.vkBindBufferMemory(device, m_buffer, m_memory.handle(), m_memory.offset());
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkBindBufferMemory");
}

VulkanBuffer::~VulkanBuffer() noexcept
{
	auto &backend = VulkanBackend::backend();
	VkDevice device = *backend.device();
	backend.vkDestroyBuffer(device, m_buffer, VulkanHostAllocator::callbacks());
}

}
