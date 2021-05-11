#include <voxen/client/vulkan/memory.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>
#include <voxen/client/vulkan/physical_device.hpp>

#include <voxen/util/log.hpp>

namespace voxen::client::vulkan
{

// --- DeviceMemory ---

DeviceMemory::DeviceMemory(const VkMemoryAllocateInfo &info)
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device();

	VkResult result = backend.vkAllocateMemory(device, &info, VulkanHostAllocator::callbacks(), &m_handle);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkAllocateMemory");
	}
}

DeviceMemory::~DeviceMemory() noexcept
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device();
	backend.vkFreeMemory(device, m_handle, VulkanHostAllocator::callbacks());
	Log::trace("Freed device memory block 0x{:X}", uintptr_t(m_handle));
}

void *DeviceMemory::map(VkDeviceSize offset, VkDeviceSize size)
{
	assert(!m_mapped_ptr);

	auto &backend = Backend::backend();
	VkDevice device = backend.device();

	void *ptr;
	VkResult result = backend.vkMapMemory(device, m_handle, offset, size, 0, &ptr);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkMapMemory");
	}

	m_mapped_ptr = ptr;
	return ptr;
}

void DeviceMemory::unmap()
{
	assert(m_mapped_ptr);

	auto &backend = Backend::backend();
	VkDevice device = backend.device();

	backend.vkUnmapMemory(device, m_handle);
	m_mapped_ptr = nullptr;
}

// --- DeviceAllocation ---

DeviceAllocation::DeviceAllocation(std::shared_ptr<DeviceMemory> memory, VkDeviceSize offset,
                                   VkDeviceSize size) noexcept
	: m_handle(memory->handle()), m_offset(offset), m_size(size), m_memory_block(std::move(memory))
{
}

// --- DeviceAllocator ---

DeviceAllocator::DeviceAllocator()
{
	Log::debug("Creating DeviceAllocator");
	readMemoryProperties();
	Log::debug("DeviceAllocator created successfully");
}

DeviceAllocator::~DeviceAllocator() noexcept
{
	Log::debug("Destroying DeviceAllocator");
}

DeviceAllocation DeviceAllocator::allocate(VkBuffer buffer, const ResourceAllocationInfo &info)
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device();

	const VkBufferMemoryRequirementsInfo2 request {
		.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,
		.pNext = nullptr,
		.buffer = buffer
	};

	VkMemoryDedicatedRequirements dedicated_reqs {
		.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS,
		.pNext = nullptr
	};

	VkMemoryRequirements2 reqs {
		.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
		.pNext = &dedicated_reqs
	};
	backend.vkGetBufferMemoryRequirements2(device, &request, &reqs);

	const AllocationInfo alloc_info {
		.size = reqs.memoryRequirements.size,
		.alignment = reqs.memoryRequirements.alignment,
		.acceptable_memory_types = reqs.memoryRequirements.memoryTypeBits,
		.use_case = info.use_case,
		.dedicated = info.force_dedicated || dedicated_reqs.requiresDedicatedAllocation ||
			(info.dedicated_if_preferred && dedicated_reqs.prefersDedicatedAllocation)
	};

	if constexpr (BuildConfig::kUseVulkanDebugging) {
		if (!info.dedicated_if_preferred && dedicated_reqs.prefersDedicatedAllocation) {
			Log::debug("Buffer wants to have dedicated allocation, but won't get it");
			Log::debug("Requirements were: size {}, alignment {}, memtype mask {:b}, use case '{}'",
			           alloc_info.size, alloc_info.alignment, alloc_info.acceptable_memory_types,
			           extras::enum_name(alloc_info.use_case));
		}
	}

	if (!alloc_info.dedicated) {
		return allocate(alloc_info);
	} else {
		return allocateDedicated(alloc_info, VkMemoryDedicatedAllocateInfo {
			.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
			.pNext = nullptr,
			.image = VK_NULL_HANDLE,
			.buffer = buffer
		});
	}
}

DeviceAllocation DeviceAllocator::allocate(VkImage image, const ResourceAllocationInfo &info)
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device();

	const VkImageMemoryRequirementsInfo2 request {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
		.pNext = nullptr,
		.image = image
	};

	VkMemoryDedicatedRequirements dedicated_reqs {
		.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS,
		.pNext = nullptr
	};

	VkMemoryRequirements2 reqs {
		.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
		.pNext = &dedicated_reqs
	};
	backend.vkGetImageMemoryRequirements2(device, &request, &reqs);

	const AllocationInfo alloc_info {
		.size = reqs.memoryRequirements.size,
		.alignment = reqs.memoryRequirements.alignment,
		.acceptable_memory_types = reqs.memoryRequirements.memoryTypeBits,
		.use_case = info.use_case,
		.dedicated = info.force_dedicated || dedicated_reqs.requiresDedicatedAllocation ||
			(info.dedicated_if_preferred && dedicated_reqs.prefersDedicatedAllocation)
	};

	if constexpr (BuildConfig::kUseVulkanDebugging) {
		if (!info.dedicated_if_preferred && dedicated_reqs.prefersDedicatedAllocation) {
			Log::debug("Image wants to have dedicated allocation, but won't get it");
			Log::debug("Requirements were: size {}, alignment {}, memtype mask {:b}, use case '{}'",
			           alloc_info.size, alloc_info.alignment, alloc_info.acceptable_memory_types,
			           extras::enum_name(alloc_info.use_case));
		}
	}

	if (!alloc_info.dedicated) {
		return allocate(alloc_info);
	} else {
		return allocateDedicated(alloc_info, VkMemoryDedicatedAllocateInfo {
			.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
			.pNext = nullptr,
			.image = image,
			.buffer = VK_NULL_HANDLE
		});
	}
}

DeviceAllocation DeviceAllocator::allocate(const AllocationInfo &info)
{
	if (info.dedicated) {
		return allocateDedicated(info, VkMemoryDedicatedAllocateInfo {
			.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
			.pNext = nullptr,
			.image = VK_NULL_HANDLE,
			.buffer = VK_NULL_HANDLE
		});
	}

	// TODO: support non-dedicated allocations
	Log::trace("TODO: requested non-dedicated allocation which is not implemented yet. Falling back to dedicated...");
	return allocateDedicated(info, VkMemoryDedicatedAllocateInfo {
		.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
		.pNext = nullptr,
		.image = VK_NULL_HANDLE,
		.buffer = VK_NULL_HANDLE
	});
}

DeviceAllocation DeviceAllocator::allocateDedicated(const AllocationInfo &info,
                                                    const VkMemoryDedicatedAllocateInfo &dedicated_info)
{
	const VkMemoryAllocateInfo alloc_info {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = &dedicated_info,
		.allocationSize = info.size,
		.memoryTypeIndex = selectMemoryType(info)
	};

	auto memory = std::make_shared<DeviceMemory>(alloc_info);
	Log::trace("Allocated device memory block 0x{:X}", uintptr_t(memory->handle()));
	return DeviceAllocation(std::move(memory), 0, info.size);
}

uint32_t DeviceAllocator::selectMemoryType(const AllocationInfo &info) const
{
	uint32_t best_type = UINT32_MAX;
	int32_t best_score = -1000;

	for (uint32_t i = 0; i < m_mem_props.memoryTypeCount; i++) {
		if (!(info.acceptable_memory_types & (1 << i))) {
			continue;
		}

		const auto &type = m_mem_props.memoryTypes[i];
		const int32_t score = scoreForUseCase(info.use_case, type.propertyFlags);
		if (score > best_score) {
			best_type = i;
			best_score = score;
		}
	}

	if (best_type == UINT32_MAX) {
		Log::error("Suitable memory type not found for request with memtype mask 0b{:b}, use case '{}'",
		           info.acceptable_memory_types, extras::enum_name(info.use_case));
		throw MessageException("no suitable Vulkan memory type found");
	}

	return best_type;
}

void DeviceAllocator::readMemoryProperties()
{
	auto &backend = Backend::backend();
	backend.vkGetPhysicalDeviceMemoryProperties(backend.physicalDevice(), &m_mem_props);

	if constexpr (BuildConfig::kUseVulkanDebugging) {
		Log::debug("Device has the following memory heaps:");
		for (uint32_t i = 0; i < m_mem_props.memoryHeapCount; i++) {
			constexpr double mult = 1.0 / double(1 << 20); // Bytes to megabytes

			const auto &heap = m_mem_props.memoryHeaps[i];
			Log::debug("#{} -> {:.1f} MB{}", i, double(heap.size) * mult,
			           (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) ? " DEVICE_LOCAL" : "");
		}

		Log::debug("Device has the following memory types:");
		for (uint32_t i = 0; i< m_mem_props.memoryTypeCount; i++) {
			const auto &type = m_mem_props.memoryTypes[i];
			Log::debug("#{} -> heap #{}{}{}{}", i, type.heapIndex,
			           (type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ? " DEVICE_LOCAL" : "",
			           (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ? " HOST_VISIBLE" : "",
			           (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) ? " HOST_CACHED" : "");
		}
	}
}

int32_t DeviceAllocator::scoreForUseCase(DeviceMemoryUseCase use_case, VkMemoryPropertyFlags flags) noexcept
{
	constexpr VkMemoryPropertyFlags HOST_FLAGS = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
	                                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	int32_t score;

	switch (use_case) {
	case DeviceMemoryUseCase::GpuOnly:
		// Not suitable if not DEVICE_LOCAL
		score = (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ? 0 : -1000;
		// Penalty for host-related properties (useless for GPU-only accesses)
		score -= (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ? 30 : 0;
		score -= (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ? 10 : 0;
		score -= (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) ? 10 : 0;
		break;
	case DeviceMemoryUseCase::Upload:
		// Not suitable if not HOST_VISIBLE or HOST_COHERENT
		score = ((flags & HOST_FLAGS) == HOST_FLAGS) ? 0 : -1000;
		// Penalty for DEVICE_LOCAL (no need to locate this memory in VRAM)
		score -= (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ? 30 : 0;
		// Penalty for HOST_CACHED (it's upload, only write combining is needed)
		score -= (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) ? 10 : 0;
		break;
	case DeviceMemoryUseCase::FastUpload:
		// Not suitable if not HOST_VISIBLE or HOST_COHERENT
		score = ((flags & HOST_FLAGS) == HOST_FLAGS) ? 0 : -1000;
		// Penalty for HOST_CACHED (it's upload, only write combining is needed)
		score -= (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) ? 10 : 0;
		// Bonus for DEVICE_LOCAL (as stated in use case description)
		score += (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ? 30 : 0;
		break;
	case DeviceMemoryUseCase::Readback:
		// Not suitable if not HOST_VISIBLE or HOST_COHERENT
		score = ((flags & HOST_FLAGS) == HOST_FLAGS) ? 0 : -1000;
		// Penalty for DEVICE_LOCAL (no need to locate this memory in VRAM)
		score -= (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ? 30 : 0;
		// Bonus for HOST_CACHED (prefetching/read combining is useful for readback)
		score += (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) ? 30 : 0;
		break;
	case DeviceMemoryUseCase::CpuSwap:
		score = 0;
		// Penalty for DEVICE_LOCAL (it's about swap space for VRAM after all)
		score -= (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ? 50 : 0;
		// Penalty for HOST_VISIBLE (since we don't need it)
		score -= (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ? 20 : 0;
		// Penalty for HOST_COHERENT or HOST_CACHED (to choose the least-featured memory type)
		score -= (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ? 10 : 0;
		score -= (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) ? 10 : 0;
		break;
	default:
		assert(false);
		__builtin_unreachable();
	}

	return score;
}

}

namespace extras
{

using voxen::client::vulkan::DeviceMemoryUseCase;

template<>
std::string_view enum_name(DeviceMemoryUseCase value) noexcept
{
	using namespace std::string_view_literals;

	switch (value) {
		case DeviceMemoryUseCase::GpuOnly: return "GpuOnly"sv;
		case DeviceMemoryUseCase::Upload: return "Upload"sv;
		case DeviceMemoryUseCase::FastUpload: return "FastUpload"sv;
		case DeviceMemoryUseCase::Readback: return "Readback"sv;
		case DeviceMemoryUseCase::CpuSwap: return "CpuSwap"sv;
		default: assert(false); return "[UNKNOWN]"sv;
	}
}

}
