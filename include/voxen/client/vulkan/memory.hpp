#pragma once

#include <voxen/client/vulkan/common.hpp>

namespace voxen::client
{

class VulkanDeviceMemory {
public:
	VulkanDeviceMemory(const VkMemoryAllocateInfo &info);
	VulkanDeviceMemory(VulkanDeviceMemory &&) = delete;
	VulkanDeviceMemory(const VulkanDeviceMemory &) = delete;
	VulkanDeviceMemory &operator = (VulkanDeviceMemory &&) = delete;
	VulkanDeviceMemory &operator = (const VulkanDeviceMemory &) = delete;
	~VulkanDeviceMemory() noexcept;

	operator VkDeviceMemory() const noexcept { return m_device_memory; }
private:
	VkDeviceMemory m_device_memory = VK_NULL_HANDLE;
};

class VulkanDeviceAllocator {
public:
	class Allocation {
	public:
		Allocation() = default;
		explicit Allocation(VkDeviceMemory handle, VkDeviceSize offset, VkDeviceSize size, uint32_t memory_type) noexcept;
		Allocation(Allocation &&) noexcept;
		Allocation(const Allocation &) noexcept;
		Allocation &operator = (Allocation &&) noexcept;
		Allocation &operator = (const Allocation &) noexcept;
		~Allocation() noexcept;

		bool isValid() const noexcept { return m_handle != VK_NULL_HANDLE; }

		VkDeviceMemory handle() const noexcept { return m_handle; }
		VkDeviceSize offset() const noexcept { return m_offset; }
		VkDeviceSize size() const noexcept { return m_size; }
		uint32_t memoryType() const noexcept { return m_memory_type; }
	private:
		VkDeviceMemory m_handle = VK_NULL_HANDLE;
		VkDeviceSize m_offset = 0;
		VkDeviceSize m_size = 0;
		uint32_t m_memory_type = UINT32_MAX;
		VulkanDeviceMemory *slab = nullptr;
	};

	VulkanDeviceAllocator();
	VulkanDeviceAllocator(VulkanDeviceAllocator &&) = delete;
	VulkanDeviceAllocator(const VulkanDeviceAllocator &) = delete;
	VulkanDeviceAllocator &operator = (VulkanDeviceAllocator &&) = delete;
	VulkanDeviceAllocator &operator = (const VulkanDeviceAllocator &) = delete;
	~VulkanDeviceAllocator() noexcept;

	Allocation allocate(const VkMemoryRequirements &reqs);
private:
	uint32_t m_staging_memory_type;
	uint32_t m_device_memory_type;

	VulkanDeviceMemory allocateSlab(uint32_t memory_type, VkDeviceSize bytes);
};

// A shorthand (kind of)
using VulkanDeviceAllocation = VulkanDeviceAllocator::Allocation;

}
