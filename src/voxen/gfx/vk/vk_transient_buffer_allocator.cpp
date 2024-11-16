#include <voxen/gfx/vk/vk_transient_buffer_allocator.hpp>

#include <voxen/gfx/vk/vk_device.hpp>
#include <voxen/gfx/vk/vk_error.hpp>
#include <voxen/util/hash.hpp>
#include <voxen/gfx/vk/vk_utils.hpp>

#include <extras/defer.hpp>

#include <vma/vk_mem_alloc.h>

#include <algorithm>
#include <cassert>

namespace voxen::gfx::vk
{

namespace
{

// How many frames a buffer not receiving any allocations will last before
// getting freed to reduce memory waste. Deliberately set low - this essentially
// defines how many frames can be served from one buffer before moving
// to the next one, and we don't want that number to be particularly high.
constexpr int64_t STALE_BUFFER_AGE_THRESHOLD = 8;

constexpr VkDeviceSize MIN_BUFFER_SIZE_TARGET = 1 * 1024 * 1024;
constexpr VkDeviceSize MAX_BUFFER_SIZE_TARGET = 64 * 1024 * 1024;
constexpr VkDeviceSize BUFFER_SIZE_STEP = 1 * 1024 * 1024;

} // namespace

struct TransientBufferAllocator::Buffer {
	VkBuffer vk_handle = VK_NULL_HANDLE;
	VmaAllocation vma_handle = VK_NULL_HANDLE;
	std::byte* host_pointer = nullptr;

	VkDeviceSize buffer_size = 0;
	// Allocation goes from top to bottom,
	// for empty buffer this will be equal to `buffer_size`
	VkDeviceSize allocation_top = 0;

	FrameTickId last_allocation_tick = FrameTickId::INVALID;
};

TransientBufferAllocator::TransientBufferAllocator(Device& dev) : m_dev(dev) {}

TransientBufferAllocator::~TransientBufferAllocator()
{
	for (uint32_t type = 0; type < TypeCount; type++) {
		for (auto& buffer : m_free_list[type]) {
			m_dev.enqueueDestroy(buffer.vk_handle, buffer.vma_handle);
		}

		for (auto& buffer : m_used_list[type]) {
			m_dev.enqueueDestroy(buffer.vk_handle, buffer.vma_handle);
		}
	}
}

auto TransientBufferAllocator::allocate(Type type, VkDeviceSize size, VkDeviceSize align) -> Allocation
{
	if (size == 0) [[unlikely]] {
		// You request nothing - you receive nothing
		return Allocation();
	}

	assert(type < TypeCount);
	assert(align > 0 && (align & (align - 1)) == 0);
	assert(m_current_tick_id.valid());

	auto& free_list = m_free_list[type];
	auto& used_list = m_used_list[type];

	auto iter = free_list.begin();
	while (iter != free_list.end()) {
		if (iter->allocation_top >= size) [[likely]] {
			// Enough space in this buffer, allocate from it
			iter->last_allocation_tick = m_current_tick_id;

			VkDeviceSize old_top = iter->allocation_top;
			VkDeviceSize new_top = (old_top - size) & ~(align - 1);
			iter->allocation_top = new_top;

			m_current_tick_allocated_bytes[type] += old_top - new_top;

			return Allocation {
				.buffer = iter->vk_handle,
				.buffer_offset = new_top,
				.host_pointer = iter->host_pointer ? iter->host_pointer + new_top : nullptr,
				.size = old_top - new_top,
			};
		} else {
			// Buffer exhausted, move it to used list and try the next one
			auto prev_iter = iter++;
			used_list.splice(used_list.end(), free_list, prev_iter);
		}
	}

	// Still haven't allocated? Time for a new buffer!
	addBuffer(type, size);

	assert(free_list.size() == 1);
	Buffer& buffer = free_list.front();
	assert(buffer.allocation_top >= size);

	buffer.last_allocation_tick = m_current_tick_id;

	VkDeviceSize old_top = buffer.allocation_top;
	VkDeviceSize new_top = (old_top - size) & ~(align - 1);
	buffer.allocation_top = new_top;

	m_current_tick_allocated_bytes[type] += old_top - new_top;

	return Allocation {
		.buffer = buffer.vk_handle,
		.buffer_offset = new_top,
		.host_pointer = buffer.host_pointer ? buffer.host_pointer + new_top : nullptr,
		.size = old_top - new_top,
	};
}

void TransientBufferAllocator::onFrameTickBegin(FrameTickId completed_tick, FrameTickId new_tick)
{
	// Do the same logic for each buffer type independently
	for (uint32_t type = 0; type < TypeCount; type++) {
		auto& free_list = m_free_list[type];
		auto& used_list = m_used_list[type];

		// What if allocations have stopped at all?
		// Then buffers won't make it to the used list, so check in free list as well.
		auto iter = free_list.begin();

		while (iter != free_list.end()) {
			if (iter->last_allocation_tick <= completed_tick
				&& iter->last_allocation_tick + STALE_BUFFER_AGE_THRESHOLD < new_tick) {
				// Stale + no longer used by GPU, destroy it, no enqueue needed
				vmaDestroyBuffer(m_dev.vma(), iter->vk_handle, iter->vma_handle);
				iter = free_list.erase(iter);
			} else {
				++iter;
			}
		}

		// Find buffers available for reset
		iter = used_list.begin();

		while (iter != used_list.end()) {
			if (iter->last_allocation_tick > completed_tick) {
				++iter;
				continue;
			}

			// Buffer is no longer used by GPU, can reset it
			iter->allocation_top = iter->buffer_size;

			if (iter->last_allocation_tick + STALE_BUFFER_AGE_THRESHOLD < new_tick) {
				// Stale buffer, destroy it, no enqueue needed
				vmaDestroyBuffer(m_dev.vma(), iter->vk_handle, iter->vma_handle);
				iter = used_list.erase(iter);
				continue;
			}

			// Move it to the end of the free list.
			// This way we will be constantly cycling through buffers.
			auto prev_iter = iter++;
			free_list.splice(free_list.end(), used_list, prev_iter);
		}
	}

	m_current_tick_id = new_tick;
}

void TransientBufferAllocator::onFrameTickEnd(FrameTickId /*current_tick*/)
{
	for (uint32_t type = 0; type < TypeCount; type++) {
		// Update exponential allocation average with fixed 0.5 weight factor.
		// New buffer allocations will use that as the size target.
		VkDeviceSize bytes = std::exchange(m_current_tick_allocated_bytes[type], 0);
		m_allocation_exp_average[type] = (m_allocation_exp_average[type] + bytes) / 2;
	}
}

void TransientBufferAllocator::addBuffer(Type type, VkDeviceSize min_size)
{
	// Only clamp the exponential average, not `min_size` to allow over-the-maximum allocations
	VkDeviceSize exp_average = std::clamp(m_allocation_exp_average[type], MIN_BUFFER_SIZE_TARGET,
		MAX_BUFFER_SIZE_TARGET);
	// Align up to the nearest multiple of `BUFFER_SIZE_STEP`
	VkDeviceSize target = std::max(exp_average, min_size) + BUFFER_SIZE_STEP - 1;
	VkDeviceSize size = target - target % BUFFER_SIZE_STEP;

	// `usage` is filled below
	VkBufferCreateInfo buffer_create_info {};
	buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_create_info.size = size;
	VulkanUtils::fillBufferSharingInfo(m_dev, buffer_create_info);

	VmaAllocationCreateInfo vma_alloc_info {};
	vma_alloc_info.usage = VMA_MEMORY_USAGE_AUTO;

	switch (type) {
	case TypeUpload:
		vma_alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		vma_alloc_info.requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		[[fallthrough]];
	case TypeScratch:
		buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
			| VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
			| VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
		break;
	case TypeCount:
		assert(false);
	}

	VkBuffer vk_handle = VK_NULL_HANDLE;
	VmaAllocation vma_handle = VK_NULL_HANDLE;

	VmaAllocationInfo alloc_info {};

	VkResult res = vmaCreateBuffer(m_dev.vma(), &buffer_create_info, &vma_alloc_info, &vk_handle, &vma_handle,
		&alloc_info);
	if (res != VK_SUCCESS) [[unlikely]] {
		throw VulkanException(res, "vmaCreateBuffer");
	}

	defer_fail{ vmaDestroyBuffer(m_dev.vma(), vk_handle, vma_handle); };

	auto disambig = VulkanUtils::makeHandleDisambiguationString(vk_handle);
	char name_buf[64];
	snprintf(name_buf, std::size(name_buf), "transient/buf_%s_%zuMB@%s", type == TypeUpload ? "upload" : "scratch",
		size >> 20, disambig.data());
	m_dev.setObjectName(vk_handle, name_buf);

	Buffer& buffer = m_free_list[type].emplace_front();
	buffer.vk_handle = vk_handle;
	buffer.vma_handle = vma_handle;
	buffer.host_pointer = static_cast<std::byte*>(alloc_info.pMappedData);
	buffer.buffer_size = size;
	buffer.allocation_top = size;
}

} // namespace voxen::gfx::vk
