#pragma once

#include <voxen/client/vulkan/common.hpp>

namespace voxen::client::vulkan
{

class DeviceMemory {
public:
	DeviceMemory(const VkMemoryAllocateInfo &info);
	DeviceMemory(DeviceMemory &&) = delete;
	DeviceMemory(const DeviceMemory &) = delete;
	DeviceMemory &operator = (DeviceMemory &&) = delete;
	DeviceMemory &operator = (const DeviceMemory &) = delete;
	~DeviceMemory() noexcept;

	operator VkDeviceMemory() const noexcept { return m_device_memory; }
private:
	VkDeviceMemory m_device_memory = VK_NULL_HANDLE;
};

class DeviceAllocator {
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
	};

	struct AllocationRequirements {
		VkMemoryRequirements memory_reqs;
		bool need_host_visibility;
		bool prefer_host_coherence;
		bool prefer_host_caching;
	};

	DeviceAllocator();
	DeviceAllocator(DeviceAllocator &&) = delete;
	DeviceAllocator(const DeviceAllocator &) = delete;
	DeviceAllocator &operator = (DeviceAllocator &&) = delete;
	DeviceAllocator &operator = (const DeviceAllocator &) = delete;
	~DeviceAllocator() noexcept;

	Allocation allocate(const AllocationRequirements &reqs);
private:
	DeviceMemory allocateSlab(uint32_t memory_type, VkDeviceSize bytes);
};

// Shorthands (kind of)
using DeviceAllocation = DeviceAllocator::Allocation;
using DeviceAllocationRequirements = DeviceAllocator::AllocationRequirements;

}
