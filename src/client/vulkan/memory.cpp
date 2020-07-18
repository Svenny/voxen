#include <voxen/client/vulkan/memory.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>

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

VulkanDeviceAllocation VulkanDeviceAllocator::allocate(const VkMemoryRequirements &reqs)
{
	Log::trace("Requested device allocation: {} bytes, align by {}, type mask {:b}", reqs.size, reqs.alignment, reqs.memoryTypeBits);
	// TODO: implement actual allocation stategy, this is a temporary stub
	auto &backend = VulkanBackend::backend();
	VkPhysicalDevice device = backend.device()->physDeviceHandle();

	VkPhysicalDeviceMemoryProperties mem_props;
	backend.vkGetPhysicalDeviceMemoryProperties(device, &mem_props);
	uint32_t selected_type = UINT32_MAX;
	for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
		if (reqs.memoryTypeBits & (1 << i)) {
			Log::trace("Allocating from memory type {}/heap {}", i, mem_props.memoryTypes[i].heapIndex);
			selected_type = i;
			break;
		}
	}
	if (selected_type == UINT32_MAX)
		throw MessageException("can't find suitable device memory type");

	// TODO: support non-dedicated allocations
	VkMemoryAllocateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	info.allocationSize = reqs.size;
	info.memoryTypeIndex = selected_type;

	VkDeviceMemory handle;
	VkResult result = backend.vkAllocateMemory(*backend.device(), &info, VulkanHostAllocator::callbacks(), &handle);
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkAllocateMemory");

	Log::trace("Allocated slab 0x{:X}", uint64_t(handle));
	return Allocation(handle, 0, reqs.size, selected_type);
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
