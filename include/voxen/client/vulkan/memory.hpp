#pragma once

#include <voxen/client/vulkan/common.hpp>

#include <extras/enum_utils.hpp>

#include <array>
#include <list>
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

// NOTE: this class is internal to `DeviceAllocator` implementation and is not used in any public API
class DeviceAllocationArena;

class DeviceAllocation final {
public:
	DeviceAllocation() = default;
	explicit DeviceAllocation(DeviceAllocationArena *arena, uint32_t begin, uint32_t end) noexcept;
	DeviceAllocation(DeviceAllocation &&other) noexcept;
	DeviceAllocation(const DeviceAllocation &) = delete;
	DeviceAllocation &operator=(DeviceAllocation &&other) noexcept;
	DeviceAllocation &operator=(const DeviceAllocation &) = delete;
	~DeviceAllocation() noexcept;

	VkDeviceMemory handle() const noexcept;
	VkDeviceSize offset() const noexcept { return m_begin_offset; }
	VkDeviceSize size() const noexcept { return m_end_offset - m_begin_offset; }
	// Returns host address space pointer to the beginning of this memory block.
	// NOTE: even if memory was allocated with use case which guarantees
	// host visibility this pointer can be null. Call `tryHostMap()` first.
	void *hostPointer() const noexcept;

	// If this memory was allocated with use case which guarantees host visibility, then it
	// will be mapped and host pointer will be returned (or an exception if mapping failed).
	// Otherwise (when host visibility was not guaranteed) either null or valid host pointer will be
	// returned - so it's possible to optimize for UMA platforms where all memory is host visible.
	void *tryHostMap() const;

private:
	DeviceAllocationArena *m_arena = nullptr;
	uint32_t m_begin_offset = 0;
	uint32_t m_end_offset = 0;
};

class DeviceAllocator final {
public:
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
	DeviceAllocator &operator=(DeviceAllocator &&) = delete;
	DeviceAllocator &operator=(const DeviceAllocator &) = delete;
	~DeviceAllocator() noexcept;

	// NOTE: memory is NOT automatically bound to the buffer after allocation
	DeviceAllocation allocate(VkBuffer buffer, DeviceMemoryUseCase use_case);
	// NOTE: disjoint images are not supported by this method
	// NOTE: memory is NOT automatically bound to the image after allocation
	DeviceAllocation allocate(VkImage image, DeviceMemoryUseCase use_case);
	DeviceAllocation allocate(const AllocationInfo &info);

	// Returns memory properties of the physical device, as reported by Vulkan driver.
	// These properties do not change after `DeviceAllocator` is created.
	const VkPhysicalDeviceMemoryProperties &deviceMemoryProperties() const noexcept { return m_mem_props; };

	// This method is called by `DeviceAllocationArena` when it is completely free.
	// Arena is deleted after calling this, so in some sense it's similar to `delete this`.
	// NOTE: this is an internal API exposed in public section, do not call it.
	void arenaFreeCallback(DeviceAllocationArena *arena) noexcept;

private:
	DeviceAllocation allocateInternal(const AllocationInfo &info, const VkMemoryDedicatedAllocateInfo *dedicated_info);
	uint32_t selectMemoryType(const AllocationInfo &info) const;
	VkDeviceSize getAllocSizeLimit(uint32_t memory_type) const;
	void readMemoryProperties();

	static DeviceMemoryUseCase mostLikelyUseCase(VkMemoryPropertyFlags flags) noexcept;
	static int32_t scoreForUseCase(DeviceMemoryUseCase use_case, VkMemoryPropertyFlags flags) noexcept;

	VkPhysicalDeviceMemoryProperties m_mem_props;
	std::array<VkDeviceSize, VK_MAX_MEMORY_TYPES> m_arena_sizes;
	std::array<std::list<DeviceAllocationArena>, VK_MAX_MEMORY_TYPES> m_arena_lists;
	std::list<DeviceAllocationArena> m_dedicated_arenas;
};

} // namespace voxen::client::vulkan

namespace extras
{

template<>
std::string_view enum_name(voxen::client::vulkan::DeviceMemoryUseCase value) noexcept;

}
