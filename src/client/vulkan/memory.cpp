#include <voxen/client/vulkan/memory.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>
#include <voxen/client/vulkan/physical_device.hpp>

#include <voxen/util/log.hpp>

namespace voxen::client
{

// --- VulkanDeviceMemory ---

VulkanDeviceMemory::VulkanDeviceMemory(const VkMemoryAllocateInfo &info)
{
	auto &backend = VulkanBackend::backend();
	VkDevice device = *backend.device();

	VkResult result = backend.vkAllocateMemory(device, &info, VulkanHostAllocator::callbacks(), &m_device_memory);
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkAllocateMemory");
}

VulkanDeviceMemory::~VulkanDeviceMemory() noexcept
{
	auto &backend = VulkanBackend::backend();
	VkDevice device = *backend.device();
	backend.vkFreeMemory(device, m_device_memory, VulkanHostAllocator::callbacks());
}

// --- VulkanDeviceAllocation ---

VulkanDeviceAllocation::Allocation(VkDeviceMemory handle, VkDeviceSize offset,
                                   VkDeviceSize size, uint32_t memory_type) noexcept
	: m_handle(handle), m_offset(offset), m_size(size), m_memory_type(memory_type)
{
}

VulkanDeviceAllocation::Allocation(VulkanDeviceAllocation &&other) noexcept
{
	m_handle = std::exchange(other.m_handle, VkDeviceMemory(VK_NULL_HANDLE));
	m_offset = std::exchange(other.m_offset, 0);
	m_size = std::exchange(other.m_size, 0);
	m_memory_type = std::exchange(other.m_memory_type, UINT32_MAX);
	slab = std::exchange(other.slab, nullptr);
}

VulkanDeviceAllocation &VulkanDeviceAllocation::operator = (VulkanDeviceAllocation &&other) noexcept
{
	m_handle = std::exchange(other.m_handle, VkDeviceMemory(VK_NULL_HANDLE));
	m_offset = std::exchange(other.m_offset, 0);
	m_size = std::exchange(other.m_size, 0);
	m_memory_type = std::exchange(other.m_memory_type, UINT32_MAX);
	slab = std::exchange(other.slab, nullptr);
	return *this;
}

VulkanDeviceAllocation::~Allocation() noexcept
{
	// TODO: reference counting/memory management by VulkanDeviceAllocator
	auto &backend = VulkanBackend::backend();
	VkDevice device = *backend.device();
	backend.vkFreeMemory(device, m_handle, VulkanHostAllocator::callbacks());
}

// --- VulkanDeviceAllocator ---

VulkanDeviceAllocator::VulkanDeviceAllocator()
{
	Log::debug("Creating VulkanDeviceAllocator");
	// TODO: find/select memory types
	Log::debug("VulkanDeviceAllocator created successfully");
}

VulkanDeviceAllocator::~VulkanDeviceAllocator() noexcept
{
	Log::debug("Destroying VulkanDeviceAllocator");
}

VulkanDeviceAllocation VulkanDeviceAllocator::allocate(const AllocationRequirements &reqs)
{
	const VkMemoryRequirements &mem_reqs = reqs.memory_reqs;
	Log::trace("Requested device allocation: {} bytes, align by {}, type mask {:b}",
	           mem_reqs.size, mem_reqs.alignment, mem_reqs.memoryTypeBits);
	// TODO: implement actual allocation stategy, this is a temporary stub
	auto &backend = VulkanBackend::backend();
	VkPhysicalDevice device = *backend.physicalDevice();

	VkPhysicalDeviceMemoryProperties mem_props;
	backend.vkGetPhysicalDeviceMemoryProperties(device, &mem_props);

	uint32_t selected_type = UINT32_MAX;
	int32_t selected_score = INT32_MIN;

	for (uint32_t type = 0; type < mem_props.memoryTypeCount; type++) {
		if ((mem_reqs.memoryTypeBits & (1 << type)) == 0) {
			Log::trace("Skipping type {} which is not supported by memory requirements", type);
			continue;
		}

		auto &type_info = mem_props.memoryTypes[type];

		if (reqs.need_host_visibility && !(type_info.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
			Log::trace("Skipping type {} which is not host-visible", type);
			continue;
		}

		int32_t score = 0;
		if (type_info.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
			score += 1000;
		if (type_info.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
			if (reqs.prefer_host_coherence)
				score += 100;
			else
				score -= 100; // Incoherent memory is probably more efficient when coherence is not needed
		}
		if (type_info.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) {
			if (reqs.prefer_host_caching)
				score += 100;
			else
				score -= 100; // Uncached memory is probably more efficient when caching is not needed
		}
		Log::trace("Type {}/heap {} has score {}", type, type_info.heapIndex, score);

		if (score > selected_score) {
			selected_type = type;
			selected_score = score;
		}
	}

	if (selected_type == UINT32_MAX)
		throw MessageException("can't find suitable device memory type");
	Log::trace("Allocating from memory type {}", selected_type);

	// TODO: support non-dedicated allocations
	VkMemoryAllocateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	info.allocationSize = mem_reqs.size;
	info.memoryTypeIndex = selected_type;

	VkDeviceMemory handle;
	VkResult result = backend.vkAllocateMemory(*backend.device(), &info, VulkanHostAllocator::callbacks(), &handle);
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkAllocateMemory");

	Log::trace("Allocated slab 0x{:X}", uint64_t(handle));
	return Allocation(handle, 0, mem_reqs.size, selected_type);
}

VulkanDeviceMemory VulkanDeviceAllocator::allocateSlab(uint32_t memory_type, VkDeviceSize bytes)
{
	VkMemoryAllocateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	info.allocationSize = bytes;
	info.memoryTypeIndex = memory_type;
	return VulkanDeviceMemory(info);
}

}
