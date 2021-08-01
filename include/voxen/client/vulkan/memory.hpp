#pragma once

#include <voxen/client/vulkan/common.hpp>

#include <extras/enum_utils.hpp>

#include <array>
#include <memory>

namespace voxen::client::vulkan
{

// Intended use case for a memory allocation. This will affect performance and host visibility.
enum class DeviceMemoryUseCase : uint32_t {
	// Allocation is made for resource frequently accessed by GPU.
	// Guaranteed to be DEVICE_LOCAL. Not guaranteed to be HOST_VISIBLE.
	GpuOnly,
	// Allocation is made for resource which is written by CPU and then read by GPU once.
	// For example, staging buffer for static assets uploading.
	// Guaranteed to be HOST_VISIBLE and HOST_COHERENT, will unlikely be HOST_CACHED.
	// Will unlikely be DEVICE_LOCAL.
	Upload,
	// Allocation is made for resource which is written by CPU and then read by GPU many times.
	// For example, frequent dynamic uploads like uniform buffers.
	// Guaranteed to be HOST_VISIBLE and HOST_COHERENT, will unlikely be HOST_CACHED.
	// Will likely be DEVICE_LOCAL.
	FastUpload,
	// Allocation is made for resource which is writteny by GPU and then read back by CPU.
	// For example, downloading of screenshots/statistics/other rendering feedback.
	// Guaranteed to be HOST_VISIBLE and HOST_COHERENT, will likely be HOST_CACHED as well.
	// Will unlikely be DEVICE_LOCAL.
	Readback,
	// Allocation is made for the purpose of paging/swap space when there is little GPU memory.
	// Not guaranteed to be HOST_VISIBLE. Will unlikely be DEVICE_LOCAL (this would defeat the purpose).
	CpuSwap,

	// Not a valid value
	EnumSize
};

class DeviceMemory final {
public:
	explicit DeviceMemory(const VkMemoryAllocateInfo &info);
	DeviceMemory(DeviceMemory &&) = delete;
	DeviceMemory(const DeviceMemory &) = delete;
	DeviceMemory &operator = (DeviceMemory &&) = delete;
	DeviceMemory &operator = (const DeviceMemory &) = delete;
	~DeviceMemory() noexcept;

	void *map(VkDeviceSize offset, VkDeviceSize size = VK_WHOLE_SIZE);
	void unmap();

	void *mappedPointer() noexcept { return m_mapped_ptr; }

	VkDeviceMemory handle() const noexcept { return m_handle; }
	const void *mappedPointer() const noexcept { return m_mapped_ptr; }

	operator VkDeviceMemory() const noexcept { return m_handle; }

private:
	VkDeviceMemory m_handle = VK_NULL_HANDLE;
	void *m_mapped_ptr = nullptr;
};

class DeviceAllocation final {
public:
	DeviceAllocation() = default;
	explicit DeviceAllocation(std::shared_ptr<DeviceMemory> memory, VkDeviceSize offset, VkDeviceSize size) noexcept;
	DeviceAllocation(DeviceAllocation &&) = default;
	DeviceAllocation(const DeviceAllocation &) = delete;
	DeviceAllocation &operator = (DeviceAllocation &&) = default;
	DeviceAllocation &operator = (const DeviceAllocation &) = delete;
	~DeviceAllocation() = default;

	VkDeviceMemory handle() const noexcept { return m_handle; }
	VkDeviceSize offset() const noexcept { return m_offset; }
	VkDeviceSize size() const noexcept { return m_size; }

private:
	VkDeviceMemory m_handle = VK_NULL_HANDLE;
	VkDeviceSize m_offset = 0;
	VkDeviceSize m_size = 0;
	std::shared_ptr<DeviceMemory> m_memory_block;
};

class DeviceAllocator final {
public:
	struct ResourceAllocationInfo {
		DeviceMemoryUseCase use_case;
		bool dedicated_if_preferred;
		bool force_dedicated;
	};

	struct AllocationInfo {
		// Minimal size of the allocation
		VkDeviceSize size;
		// Minimal alignment of the start of the allocation
		VkDeviceSize alignment;
		// Bitmask of Vulkan memory types acceptable for this allocation.
		// Set to UINT32_MAX to allow any memory type allowed by use case.
		uint32_t acceptable_memory_types;
		// Intended use case for this allocation
		DeviceMemoryUseCase use_case;
		// If set to `true`, then this allocation will be dedicated - that is,
		// it will be backed by a private VkDeviceMemory object
		bool dedicated;
	};

	DeviceAllocator();
	DeviceAllocator(DeviceAllocator &&) = delete;
	DeviceAllocator(const DeviceAllocator &) = delete;
	DeviceAllocator &operator = (DeviceAllocator &&) = delete;
	DeviceAllocator &operator = (const DeviceAllocator &) = delete;
	~DeviceAllocator() noexcept;

	// NOTE: memory is NOT automatically bound to the buffer after allocation
	DeviceAllocation allocate(VkBuffer buffer, const ResourceAllocationInfo &info);
	// NOTE: disjoint images are not supported by this method
	// NOTE: memory is NOT automatically bound to the image after allocation
	DeviceAllocation allocate(VkImage image, const ResourceAllocationInfo &info);
	DeviceAllocation allocate(const AllocationInfo &info);

private:
	DeviceAllocation allocateDedicated(const AllocationInfo &info, const VkMemoryDedicatedAllocateInfo &dedicated_info);
	uint32_t selectMemoryType(const AllocationInfo &info) const;
	void readMemoryProperties();

	static int32_t scoreForUseCase(DeviceMemoryUseCase use_case, VkMemoryPropertyFlags flags) noexcept;

	VkPhysicalDeviceMemoryProperties m_mem_props;
};

}

namespace extras
{

template<>
std::string_view enum_name(voxen::client::vulkan::DeviceMemoryUseCase value) noexcept;

}
