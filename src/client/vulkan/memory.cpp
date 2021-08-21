#include <voxen/client/vulkan/memory.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/config.hpp>
#include <voxen/client/vulkan/device.hpp>
#include <voxen/client/vulkan/physical_device.hpp>

#include <voxen/util/log.hpp>

#include <vector>

namespace voxen::client::vulkan
{

// Memory type having this or lower score is considered unsupported for use case in question
constexpr static int32_t WORST_MEMORY_TYPE_SCORE = -1000;

static double toMegabytes(VkDeviceSize size) noexcept
{
	constexpr double mult = 1.0 / double(1u << 20u);
	return double(size) * mult;
}

// --- DeviceAllocationArena ---

class DeviceAllocationArena final {
public:
	using Range = std::pair<uint32_t, uint32_t>;

	explicit DeviceAllocationArena(VkDeviceSize size, uint32_t memory_type,
	                               const VkMemoryDedicatedAllocateInfo *dedicated_info)
		: m_full_size(static_cast<uint32_t>(size)), m_memory_type(memory_type), m_dedicated(dedicated_info != nullptr)
	{
		// We use 32-bit offsets in free blocks for more compact storage.
		// Limit to 2^31 (2 GB) for extra safety margin in offset arithmetic.
		// Arenas shouldn't be that big anyway (generally around 32-128 MB).
		assert(size <= 1u << 31u);

		if (!m_dedicated) {
			// Reserving this for dedicated arenas would be just a waste of memory
			m_free_ranges.reserve(Config::EXPECTED_PER_ARENA_ALLOCATIONS);
		}

		// Initially we have all arena covered by a single free block
		m_free_ranges.emplace_back(0, m_full_size);

		auto &backend = Backend::backend();

		const auto &mem_props = backend.deviceAllocator().deviceMemoryProperties().memoryTypes[memory_type];
		m_host_mappable = !!(mem_props.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		const VkMemoryAllocateInfo info {
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.pNext = dedicated_info,
			.allocationSize = size,
			.memoryTypeIndex = memory_type
		};
		VkResult result = backend.vkAllocateMemory(backend.device(), &info, HostAllocator::callbacks(), &m_handle);
		if (result != VK_SUCCESS) {
			throw VulkanException(result, "vkAllocateMemory");
		}
	}

	DeviceAllocationArena(DeviceAllocationArena &&other) noexcept
	{
		*this = std::move(other);
	}

	DeviceAllocationArena(const DeviceAllocationArena &) = delete;

	DeviceAllocationArena &operator = (DeviceAllocationArena &&other) noexcept
	{
		std::swap(m_handle, other.m_handle);
		std::swap(m_host_pointer, other.m_host_pointer);
		std::swap(m_free_ranges, other.m_free_ranges);
		std::swap(m_full_size, other.m_full_size);
		std::swap(m_memory_type, other.m_memory_type);
		std::swap(m_host_mappable, other.m_host_mappable);
		std::swap(m_dedicated, other.m_dedicated);
		return *this;
	}

	DeviceAllocationArena &operator = (const DeviceAllocationArena &) = delete;

	~DeviceAllocationArena() noexcept
	{
		auto &backend = Backend::backend();
		VkDevice device = backend.device();
		backend.vkFreeMemory(device, m_handle, HostAllocator::callbacks());
	}

	void map()
	{
		assert(!m_host_pointer);
		assert(m_host_mappable);

		auto &backend = Backend::backend();
		VkResult result = backend.vkMapMemory(backend.device(), m_handle, 0, VK_WHOLE_SIZE, 0, &m_host_pointer);
		if (result != VK_SUCCESS) {
			throw VulkanException(result, "vkMapMemory");
		}
	}

	void tryMap()
	{
		if (m_host_pointer || !m_host_mappable) {
			return;
		}

		map();
	}

	void unmap()
	{
		assert(m_host_pointer);

		auto &backend = Backend::backend();
		backend.vkUnmapMemory(backend.device(), m_handle);
		m_host_pointer = nullptr;
	}

	DeviceAllocation allocateDedicated() noexcept
	{
		assert(m_dedicated);
		assert(isFree());

		m_free_ranges.clear();
		return DeviceAllocation(this, 0, m_full_size);
	}

	std::optional<DeviceAllocation> allocate(VkDeviceSize size, VkDeviceSize align)
	{
		align = std::max(align, Config::ALLOCATION_GRANULARITY);
		size = VulkanUtils::alignUp(size, Config::ALLOCATION_GRANULARITY);

		if (size > m_full_size) [[unlikely]] {
			// Outright reject unfeasibly big requests
			return std::nullopt;
		}

		const auto size_u32 = static_cast<uint32_t>(size);
		const auto align_u32 = static_cast<uint32_t>(align);

		for (size_t i = 0; i < m_free_ranges.size(); i++) {
			const uint32_t begin = m_free_ranges[i].first;
			const uint32_t end = m_free_ranges[i].second;

			const uint32_t adjusted_begin = VulkanUtils::alignUp(begin, align_u32);
			const uint32_t adjusted_end = adjusted_begin + size_u32;

			if (adjusted_end > end) {
				continue;
			}

			// Ensure there is room for at least two more objects.
			// First one can be emplaced down the code. If allocation fails here,
			// we won't modify the vector, thus ensuring strong exception safety.
			// Second one can be emplaced during `free(adjusted_begin, adjusted_end)`,
			// so this reservation guarantees that `free()` is actually noexcept.
			// TODO (Svenny): but it seems to force linear (not exponential) capacity growth.
			m_free_ranges.reserve(m_free_ranges.size() + 2);

			bool used_inplace = false;

			if (adjusted_end < end) {
				m_free_ranges[i] = Range(adjusted_end, end);
				used_inplace = true;
			}

			if (adjusted_begin > begin) {
				if (!used_inplace) {
					m_free_ranges[i] = Range(begin, adjusted_begin);
					used_inplace = true;
				} else {
					m_free_ranges.emplace(m_free_ranges.begin() + i, begin, adjusted_begin);
				}
			}

			if (!used_inplace) {
				m_free_ranges.erase(m_free_ranges.begin() + i);
			}

			return DeviceAllocation(this, adjusted_begin, adjusted_end);
		}

		return std::nullopt;
	}

	void free(Range range) noexcept
	{
		const uint32_t begin = range.first;
		const uint32_t end = range.second;
		assert(begin < end);

		// Find the first "higher" free block and insert a new one right before it
		auto iter = std::upper_bound(m_free_ranges.begin(), m_free_ranges.end(), std::make_pair(begin, end));
		// This can't throw as `allocate` has reserved space for this block in advance
		iter = m_free_ranges.emplace(iter, begin, end);

		if (auto next = iter + 1; next != m_free_ranges.end()) {
			assert(next->first >= end);
			if (next->first == end) {
				// The next free range starts right at the end of just added one - merge them
				iter->second = next->second;
				// This can't invalidate `iter`, as well as all iterators before it
				m_free_ranges.erase(next);
			}
		}

		if (iter != m_free_ranges.begin()) {
			auto prev = iter - 1;
			assert(prev->second <= begin);
			if (prev->second == begin) {
				// The previous free range ends right at the start of just added one - merge them
				iter->first = prev->first;
				// This will invalidate `iter`, but we are not going to use it anymore
				m_free_ranges.erase(prev);
			}
		}

		if (isFree()) [[unlikely]] {
			// Don't do anything after this call, `this` is probably destroyed in it
			Backend::backend().deviceAllocator().arenaFreeCallback(this);
		}
	}

	bool isFree() const noexcept
	{
		return m_free_ranges.size() == 1 && m_free_ranges[0] == Range(0, m_full_size);
	}

	VkDeviceMemory handle() const noexcept { return m_handle; }
	void *hostPointer() const noexcept { return m_host_pointer; }
	VkDeviceSize fullSize() const noexcept { return m_full_size; }
	uint32_t memoryType() const noexcept { return m_memory_type; }
	bool isDedicated() const noexcept { return m_dedicated; }

private:
	VkDeviceMemory m_handle = VK_NULL_HANDLE;
	void *m_host_pointer = nullptr;
	// Holds [begin; end) pairs of offsets into the allocation.
	// This vector is always ordered by both fields of its elements, that is,
	// i+1'th element has both `.first` and `.second` greater than in i'th.
	std::vector<Range> m_free_ranges;
	uint32_t m_full_size = 0;
	uint32_t m_memory_type = 0;
	bool m_host_mappable = false;
	bool m_dedicated = false;
};

// --- DeviceAllocation ---

DeviceAllocation::DeviceAllocation(DeviceAllocationArena *arena, uint32_t begin, uint32_t end) noexcept
	: m_arena(arena), m_begin_offset(begin), m_end_offset(end)
{}

DeviceAllocation::DeviceAllocation(DeviceAllocation &&other) noexcept
{
	*this = std::move(other);
}

DeviceAllocation &DeviceAllocation::operator = (DeviceAllocation &&other) noexcept
{
	std::swap(m_arena, other.m_arena);
	std::swap(m_begin_offset, other.m_begin_offset);
	std::swap(m_end_offset, other.m_end_offset);
	return *this;
}

DeviceAllocation::~DeviceAllocation() noexcept
{
	if (m_arena) {
		m_arena->free(DeviceAllocationArena::Range(m_begin_offset, m_end_offset));
	}
}

VkDeviceMemory DeviceAllocation::handle() const noexcept
{
	return m_arena->handle();
}

void *DeviceAllocation::hostPointer() const noexcept
{
	uintptr_t p = reinterpret_cast<uintptr_t>(m_arena->hostPointer());
	if (!p) {
		return nullptr;
	}

	return reinterpret_cast<void *>(p + m_begin_offset);
}

void *DeviceAllocation::tryHostMap() const
{
	m_arena->tryMap();
	return hostPointer();
}

// --- DeviceAllocator ---

DeviceAllocator::DeviceAllocator()
{
	Log::debug("Creating DeviceAllocator");
	readMemoryProperties();
	m_arena_sizes.fill(Config::ARENA_SIZE_INITIAL_GUESS);
	Log::debug("DeviceAllocator created successfully");
}

DeviceAllocator::~DeviceAllocator() noexcept
{
	Log::debug("Destroying DeviceAllocator");
}

DeviceAllocation DeviceAllocator::allocate(VkBuffer buffer, DeviceMemoryUseCase use_case)
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
		.use_case = use_case,
		.dedicated = dedicated_reqs.prefersDedicatedAllocation == VK_TRUE
	};

	if (!alloc_info.dedicated) {
		return allocateInternal(alloc_info, nullptr);
	}

	const VkMemoryDedicatedAllocateInfo dedicated_info {
		.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
		.pNext = nullptr,
		.image = VK_NULL_HANDLE,
		.buffer = buffer
	};
	return allocateInternal(alloc_info, &dedicated_info);
}

DeviceAllocation DeviceAllocator::allocate(VkImage image, DeviceMemoryUseCase use_case)
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
		.use_case = use_case,
		.dedicated = dedicated_reqs.prefersDedicatedAllocation == VK_TRUE
	};

	if (!alloc_info.dedicated) {
		return allocateInternal(alloc_info, nullptr);
	}

	const VkMemoryDedicatedAllocateInfo dedicated_info {
		.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
		.pNext = nullptr,
		.image = image,
		.buffer = VK_NULL_HANDLE
	};
	return allocateInternal(alloc_info, &dedicated_info);
}

DeviceAllocation DeviceAllocator::allocate(const AllocationInfo &info)
{
	if (!info.dedicated) {
		return allocateInternal(info, nullptr);
	}

	const VkMemoryDedicatedAllocateInfo dedicated_info {
		.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
		.pNext = nullptr,
		.image = VK_NULL_HANDLE,
		.buffer = VK_NULL_HANDLE
	};
	return allocateInternal(info, &dedicated_info);
}

void DeviceAllocator::arenaFreeCallback(DeviceAllocationArena *arena) noexcept
{
	assert(arena);

	auto &arena_list = arena->isDedicated() ? m_dedicated_arenas : m_arena_lists[arena->memoryType()];
	for (auto iter = arena_list.begin(); iter != arena_list.end(); ++iter) {
		if (&(*iter) == arena) {
			Log::debug("Freeing {}arena with memory type #{}, size {:.1f} MB", arena->isDedicated() ? "dedicated " : "",
				arena->memoryType(), toMegabytes(arena->fullSize()));
			m_dedicated_arenas.erase(iter);
			return;
		}
	}

	// Arena must be in the list
	assert(false);
	__builtin_unreachable();
}

DeviceAllocation DeviceAllocator::allocateInternal(const AllocationInfo &info,
                                                   const VkMemoryDedicatedAllocateInfo *dedicated_info)
{
	const uint32_t memory_type = selectMemoryType(info);
	const VkDeviceSize size_limit = getAllocSizeLimit(memory_type);

	if (info.size >= size_limit) [[unlikely]] {
		Log::error("Requested allocation of size {:.1f} MB is too big for memory type #{} (limit is {:.1f} MB)",
			toMegabytes(info.size), memory_type, toMegabytes(size_limit));
		throw MessageException("refusing too big allocation");
	}

	if (dedicated_info) {
		m_dedicated_arenas.emplace_back(info.size, memory_type, dedicated_info);
		return m_dedicated_arenas.back().allocateDedicated();
	}

	auto &arena_list = m_arena_lists[memory_type];
	for (auto &arena : arena_list) {
		std::optional<DeviceAllocation> maybe_alloc = arena.allocate(info.size, info.alignment);
		if (maybe_alloc.has_value()) {
			return *std::move(maybe_alloc);
		}
	}

	// All arenas for this memory type are exhausted, need to add a new one

	if (m_arena_sizes[memory_type] < info.size) {
		// Requested allocation is bigger than target arena size. This means we are underestimating
		// the needed arena size - it should fit a lot of allocations, so adjusting size seems sane.
		Log::debug("Requested allocation {:.1f} MB exceeds arena size (was {:.1f} MB) for memory type #{}, adjusting it",
			toMegabytes(info.size), toMegabytes(m_arena_sizes[memory_type]), memory_type);
		m_arena_sizes[memory_type] = VulkanUtils::alignUp(info.size, Config::ARENA_SIZE_ALIGNMENT);
	}

	if (!arena_list.empty()) {
		// Grow arena size based on previous estimates
		VkDeviceSize avg_size = 0;
		for (const auto &arena : arena_list) {
			avg_size += arena.fullSize();
		}
		avg_size /= arena_list.size();

		VkDeviceSize new_size = VulkanUtils::calcFraction(avg_size, Config::ARENA_GROW_FACTOR_NUMERATOR,
			Config::ARENA_GROW_FACTOR_DENOMINATOR);
		new_size = VulkanUtils::alignUp(new_size, Config::ARENA_SIZE_ALIGNMENT);

		m_arena_sizes[memory_type] = std::max(m_arena_sizes[memory_type], new_size);
	}

	const VkDeviceSize new_arena_size = m_arena_sizes[memory_type];
	Log::debug("Adding a new arena (size {:.1f} MB) for memory type #{}", toMegabytes(new_arena_size), memory_type);
	arena_list.emplace_back(new_arena_size, memory_type, nullptr);

	auto &arena = arena_list.back();
	std::optional<DeviceAllocation> maybe_alloc = arena.allocate(info.size, info.alignment);
	// Allocation must succeed - previous logic has ensured new arena will fit at least one requested object
	assert(maybe_alloc.has_value());
	return *std::move(maybe_alloc);
}

uint32_t DeviceAllocator::selectMemoryType(const AllocationInfo &info) const
{
	uint32_t best_type = UINT32_MAX;
	int32_t best_score = WORST_MEMORY_TYPE_SCORE;

	for (uint32_t i = 0; i < m_mem_props.memoryTypeCount; i++) {
		if (!(info.acceptable_memory_types & (1u << i))) {
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

VkDeviceSize DeviceAllocator::getAllocSizeLimit(uint32_t memory_type) const
{
	uint32_t heap = m_mem_props.memoryTypes[memory_type].heapIndex;
	// For an extra safety margin allow allocating up to one quarter of the heap at a time
	return m_mem_props.memoryHeaps[heap].size / 4;
}

void DeviceAllocator::readMemoryProperties()
{
	auto &backend = Backend::backend();
	backend.vkGetPhysicalDeviceMemoryProperties(backend.physicalDevice(), &m_mem_props);

	if (!Log::willBeLogged(Log::Level::Debug)) {
		return;
	}

	Log::debug("Device has the following memory heaps:");
	for (uint32_t i = 0; i < m_mem_props.memoryHeapCount; i++) {
		const auto &heap = m_mem_props.memoryHeaps[i];
		Log::debug("#{} -> {:.1f} MB{}", i, toMegabytes(heap.size),
			(heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) ? " [device local]" : "");
	}

	Log::debug("Device has the following memory types:");
	for (uint32_t i = 0; i < m_mem_props.memoryTypeCount; i++) {
		const auto &type = m_mem_props.memoryTypes[i];
		Log::debug("#{} -> likely '{}' use case, heap #{}{}{}{}",
			i, extras::enum_name(mostLikelyUseCase(type.propertyFlags)), type.heapIndex,
			(type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ? " [device local]" : "",
			(type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ? " [host visible]" : "",
			(type.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) ? " [host cached]" : "");
	}
}

DeviceMemoryUseCase DeviceAllocator::mostLikelyUseCase(VkMemoryPropertyFlags flags) noexcept
{
	// This use case has no specific requirements and can be treated
	// as "sink" for any memory type we couldn't properly classify
	DeviceMemoryUseCase best_use_case = DeviceMemoryUseCase::CpuSwap;
	int32_t best_score = WORST_MEMORY_TYPE_SCORE;

	for (uint32_t i = 0; i < extras::enum_size_v<DeviceMemoryUseCase>; i++) {
		DeviceMemoryUseCase use_case { i };
		int32_t score = scoreForUseCase(use_case, flags);

		if (score > best_score) {
			best_use_case = use_case;
			best_score = score;
		}
	}

	return best_use_case;
}

int32_t DeviceAllocator::scoreForUseCase(DeviceMemoryUseCase use_case, VkMemoryPropertyFlags flags) noexcept
{
	constexpr VkMemoryPropertyFlags HOST_FLAGS = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
	                                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	int32_t score;

	switch (use_case) {
	case DeviceMemoryUseCase::GpuOnly:
		// Not suitable if not DEVICE_LOCAL
		score = (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ? 0 : WORST_MEMORY_TYPE_SCORE;
		// Penalty for host-related properties (useless for GPU-only accesses)
		score -= (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ? 30 : 0;
		score -= (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ? 10 : 0;
		score -= (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) ? 10 : 0;
		break;
	case DeviceMemoryUseCase::Upload:
		// Not suitable if not HOST_VISIBLE or HOST_COHERENT
		score = ((flags & HOST_FLAGS) == HOST_FLAGS) ? 0 : WORST_MEMORY_TYPE_SCORE;
		// Penalty for DEVICE_LOCAL (no need to locate this memory in VRAM)
		score -= (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ? 30 : 0;
		// Penalty for HOST_CACHED (it's upload, only write combining is needed)
		score -= (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) ? 10 : 0;
		break;
	case DeviceMemoryUseCase::FastUpload:
		// Not suitable if not HOST_VISIBLE or HOST_COHERENT
		score = ((flags & HOST_FLAGS) == HOST_FLAGS) ? 0 : WORST_MEMORY_TYPE_SCORE;
		// Penalty for HOST_CACHED (it's upload, only write combining is needed)
		score -= (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) ? 10 : 0;
		// Bonus for DEVICE_LOCAL (as stated in use case description)
		score += (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ? 30 : 0;
		break;
	case DeviceMemoryUseCase::Readback:
		// Not suitable if not HOST_VISIBLE or HOST_COHERENT
		score = ((flags & HOST_FLAGS) == HOST_FLAGS) ? 0 : WORST_MEMORY_TYPE_SCORE;
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
		default:
			assert(false);
			return "[UNKNOWN]"sv;
	}
}

}
