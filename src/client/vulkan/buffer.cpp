#include <voxen/client/vulkan/buffer.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>

#include <extras/defer.hpp>

namespace voxen::client::vulkan
{

Buffer::Buffer(const VkBufferCreateInfo &info, Usage usage)
{
	auto &backend = VulkanBackend::backend();
	VkDevice device = *backend.device();
	auto allocator = VulkanHostAllocator::callbacks();

	VkResult result = backend.vkCreateBuffer(device, &info, allocator, &m_buffer);
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkCreateBuffer");
	defer_fail { backend.vkDestroyBuffer(device, m_buffer, allocator); };

	DeviceAllocationRequirements reqs = {};
	backend.vkGetBufferMemoryRequirements(device, m_buffer, &reqs.memory_reqs);
	switch (usage) {
	case Usage::DeviceLocal:
		// Don't need any host-side properties
		reqs.need_host_visibility = false;
		break;
	case Usage::Staging:
		// Don't need host caching (write combiner is enough for this usage),
		// but having coherence is preferred as it allows to avoid
		reqs.need_host_visibility = true;
		reqs.prefer_host_coherence = true;
		reqs.prefer_host_caching = false;
		break;
	case Usage::Readback:
		// Don't need host coherence (we may afford a heavy flush/invalidate for
		// readback), but caching is preferred (because of cache prefetcher?)
		reqs.need_host_visibility = true;
		reqs.prefer_host_coherence = false;
		reqs.prefer_host_caching = true;
		break;
	}
	m_memory = backend.deviceAllocator()->allocate(reqs);

	result = backend.vkBindBufferMemory(device, m_buffer, m_memory->handle(), m_memory->offset());
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkBindBufferMemory");
}

Buffer::~Buffer() noexcept
{
	auto &backend = VulkanBackend::backend();
	VkDevice device = *backend.device();
	backend.vkDestroyBuffer(device, m_buffer, VulkanHostAllocator::callbacks());
}

}
