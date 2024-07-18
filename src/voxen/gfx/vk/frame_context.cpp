#include <voxen/gfx/vk/frame_context.hpp>

#include <voxen/client/vulkan/common.hpp>
#include <voxen/gfx/vk/vk_device.hpp>
#include <voxen/gfx/vk/vk_instance.hpp>
#include <voxen/util/error_condition.hpp>
#include <voxen/util/log.hpp>

#include "vk_private_consts.hpp"

#include <extras/defer.hpp>

#include <vma/vk_mem_alloc.h>

namespace voxen::gfx::vk
{

// TODO: these parts are not yet moved to voxen/gfx/vk
using client::vulkan::VulkanException;
using client::vulkan::VulkanUtils;

struct FrameContext::ConstantUploadBuffer {
	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
	std::span<std::byte> host_mapped_span;
	std::span<std::byte> remaining_free_span;
};

// FrameContext

FrameContext::FrameContext(Device &device) : m_device(device)
{
	VkDevice dev = device.handle();
	auto &dt = device.dt();

	VkCommandPoolCreateInfo cmd_pool_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.queueFamilyIndex = device.info().main_queue_family,
	};

	VkResult res = dt.vkCreateCommandPool(dev, &cmd_pool_info, nullptr, &m_cmd_pool);
	if (res != VK_SUCCESS) {
		throw VulkanException(res, "vkCreateCommandPool");
	}
	defer_fail { dt.vkDestroyCommandPool(dev, m_cmd_pool, nullptr); };

	VkCommandBufferAllocateInfo cmd_buf_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext = nullptr,
		.commandPool = m_cmd_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};

	dt.vkAllocateCommandBuffers(dev, &cmd_buf_info, &m_cmd_buffer);
	if (res != VK_SUCCESS) {
		throw VulkanException(res, "vkAllocateCommandBuffer");
	}

	m_ubo_offset_align = m_device.physicalDevice().info().props.properties.limits.minUniformBufferOffsetAlignment;
}

FrameContext::~FrameContext() noexcept
{
	for (auto &pool : m_descriptor_pools) {
		m_device.enqueueDestroy(pool);
	}

	for (auto &buf : m_const_upload_buffers) {
		m_device.enqueueDestroy(buf.buffer, buf.allocation);
	}

	m_device.enqueueDestroy(m_cmd_pool);
}

VkDescriptorSet FrameContext::allocateDescriptorSet(VkDescriptorSetLayout layout)
{
	if (m_descriptor_pools.empty()) {
		addDescriptorPool();
	}

	// Try allocating from the last pool
	VkDescriptorSetAllocateInfo alloc_info {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = nullptr,
		.descriptorPool = VK_NULL_HANDLE,
		.descriptorSetCount = 1,
		.pSetLayouts = &layout,
	};

	VkDescriptorSet set = VK_NULL_HANDLE;

	if (!m_descriptor_pools.empty()) {
		// First, try allocating from the existing set
		alloc_info.descriptorPool = m_descriptor_pools.back();

		VkResult res = m_device.dt().vkAllocateDescriptorSets(m_device.handle(), &alloc_info, &set);
		if (res == VK_SUCCESS) {
			return set;
		}

		if (res != VK_ERROR_OUT_OF_POOL_MEMORY) {
			// It's not pool exhaustion - something unexpected
			throw VulkanException(res, "vkAllocateDescriptorSets");
		}
	}

	// Existing pools exhausted, allocate a new one
	addDescriptorPool();
	alloc_info.descriptorPool = m_descriptor_pools.back();

	VkResult res = m_device.dt().vkAllocateDescriptorSets(m_device.handle(), &alloc_info, &set);
	if (res == VK_SUCCESS) {
		return set;
	}

	if (res != VK_ERROR_OUT_OF_POOL_MEMORY) {
		// It's not pool exhaustion - something unexpected
		throw VulkanException(res, "vkAllocateDescriptorSets");
	}

	// A new pool is already exhausted - this is not expected,
	// we will not try increasing the pool size.
	Log::error("Requested allocation of descriptor set exceeding the whole pool!");
	throw Exception::fromError(VoxenErrc::GfxFailure, "descriptor set layout is too big");
}

FrameContext::ConstantUpload FrameContext::allocateConstantUpload(VkDeviceSize size)
{
	// Pre-align the size to not deal with it later
	size = VulkanUtils::alignUp(size, m_ubo_offset_align);

	// Look through all buffers. This might look quite suboptimal
	// but we do buffer fusing (see `waitAndReset()`), so after
	// a few frames there will be just one buffer.
	for (auto &buf : m_const_upload_buffers) {
		if (buf.remaining_free_span.size_bytes() >= size) {
			// Found free space in existing buffer
			ConstantUpload result {
				.buffer = buf.buffer,
				.offset = VkDeviceSize(buf.remaining_free_span.data() - buf.host_mapped_span.data()),
				.host_mapped_span = buf.remaining_free_span.subspan(0, size),
			};
			// We guarantee alignment by pre-aligning the size
			assert(result.offset % m_ubo_offset_align == 0);
			// Reduce the remaining free span
			buf.remaining_free_span = buf.remaining_free_span.subspan(size);
			return result;
		}
	}

	// No free space (or no buffers), need to allocate
	VkDeviceSize new_size = std::max(size, Consts::CONST_UPLOAD_BUFFER_STARTING_SIZE);
	if (!m_const_upload_buffers.empty()) {
		// Not the first buffer - apply grow factor
		new_size = std::max(new_size, m_const_upload_buffers.back().host_mapped_span.size_bytes());
		new_size = Consts::growConstUploadBufferSize(new_size);
	}

	addConstantUploadBuffer(new_size);

	auto &buf = m_const_upload_buffers.back();
	ConstantUpload result {
		.buffer = buf.buffer,
		.offset = 0,
		.host_mapped_span = buf.remaining_free_span.subspan(0, size),
	};
	// Reduce the remaining free span
	buf.remaining_free_span = buf.remaining_free_span.subspan(size);
	return result;
}

uint64_t FrameContext::submit(VkSemaphore wait_binary_semaphore, VkSemaphore signal_binary_semaphore)
{
	VkResult res = m_device.dt().vkEndCommandBuffer(m_cmd_buffer);
	if (res != VK_SUCCESS) {
		throw VulkanException(res, "vkEndCommandBuffer");
	}

	m_submit_timeline = m_device.submitCommands({
		.queue = Device::QueueMain,
		.wait_binary_semaphore = wait_binary_semaphore,
		.cmds = std::span(&m_cmd_buffer, 1),
		.signal_binary_semaphore = signal_binary_semaphore,
	});

	return m_submit_timeline;
}

void FrameContext::waitAndReset()
{
	m_device.waitForTimeline(Device::QueueMain, m_submit_timeline);

	VkDevice dev = m_device.handle();
	auto &dt = m_device.dt();

	VkResult res = dt.vkResetCommandPool(dev, m_cmd_pool, 0);
	if (res != VK_SUCCESS) {
		throw VulkanException(res, "vkResetCommandPool");
	}

	VkCommandBufferBeginInfo begin_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = nullptr,
	};

	res = dt.vkBeginCommandBuffer(m_cmd_buffer, &begin_info);
	if (res != VK_SUCCESS) {
		throw VulkanException(res, "vkBeginCommandBuffer");
	}

	// Reset all descriptor pools
	for (auto &pool : m_descriptor_pools) {
		res = dt.vkResetDescriptorPool(dev, pool, 0);
		if (res != VK_SUCCESS) {
			throw VulkanException(res, "vkResetDescriptorPool");
		}
	}

	// Fuse multiple const upload buffers. This might happen several times
	// before reaching a certain stable "high water" level. We do not attempt
	// reducing the size, as large constant upload "spikes" are not expected.
	if (m_const_upload_buffers.size() > 1) {
		VkDeviceSize previously_used_bytes = 0;

		for (auto &buf : m_const_upload_buffers) {
			previously_used_bytes += buf.host_mapped_span.size_bytes() - buf.remaining_free_span.size_bytes();
			m_device.enqueueDestroy(buf.buffer, buf.allocation);
		}

		m_const_upload_buffers.clear();
		// Add some extra bytes to slightly speed up the convergence
		VkDeviceSize new_size = Consts::addConstUploadBufferFusing(previously_used_bytes);
		addConstantUploadBuffer(new_size);
		// Might help with constants tuning until we have proper stats reporting
		Log::debug("Fused constant upload buffers to {} KiB", new_size);
	} else if (!m_const_upload_buffers.empty()) {
		// Reset allocated span
		auto &buf = m_const_upload_buffers.back();
		buf.remaining_free_span = buf.host_mapped_span;
	}
}

void FrameContext::addDescriptorPool()
{
	VkDescriptorPoolInlineUniformBlockCreateInfo inlub_create_info {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO,
		.pNext = nullptr,
		// Assuming one INLUB per set (not much sense to have more)
		.maxInlineUniformBlockBindings = Consts::DESCRIPTOR_POOL_SCALE_FACTOR,
	};

	VkDescriptorPoolCreateInfo create_info {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.pNext = &inlub_create_info,
		.flags = 0,
		.maxSets = Consts::DESCRIPTOR_POOL_SCALE_FACTOR,
		.poolSizeCount = std::size(Consts::DESCRIPTOR_POOL_SIZING),
		.pPoolSizes = Consts::DESCRIPTOR_POOL_SIZING,
	};

	VkDescriptorPool pool = VK_NULL_HANDLE;
	VkResult res = m_device.dt().vkCreateDescriptorPool(m_device.handle(), &create_info, nullptr, &pool);
	if (res != VK_SUCCESS) {
		throw VulkanException(res, "vkCreateDescriptorPool");
	}
	defer_fail { m_device.dt().vkDestroyDescriptorPool(m_device.handle(), pool, nullptr); };

	// Insert new entry only after successful pool creation
	// so that the array will always contain only valid handles
	m_descriptor_pools.emplace_back(pool);

	if (size_t size = m_descriptor_pools.size(); size > 1) {
		// Might help with constants tuning until we have proper stats reporting
		Log::debug("Added new descriptor pool, now have {}", size);
	}
}

void FrameContext::addConstantUploadBuffer(VkDeviceSize size)
{
	// Slice allocation sizes are always multiples of UBO offset
	// alignment, make this too to not waste some last bytes
	size = VulkanUtils::alignUp(size, m_ubo_offset_align);

	VmaAllocator vma = m_device.vma();

	VkBufferCreateInfo create_info {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.size = size,
		.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
	};

	VmaAllocationCreateInfo alloc_create_info {};
	alloc_create_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	// Place it in BAR (PCI aperture) if possible
	alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation alloc = VK_NULL_HANDLE;
	VmaAllocationInfo alloc_info {};

	VkResult res = vmaCreateBuffer(vma, &create_info, &alloc_create_info, &buffer, &alloc, &alloc_info);
	if (res != VK_SUCCESS) {
		throw VulkanException(res, "vmaCreateBuffer");
	}
	defer_fail { vmaDestroyBuffer(vma, buffer, alloc); };

	// Insert new entry only after successful buffer creation
	// so that the array will always contain only valid handles
	ConstantUploadBuffer &buf = m_const_upload_buffers.emplace_back();
	buf.buffer = buffer;
	buf.allocation = alloc;
	buf.host_mapped_span = std::span(static_cast<std::byte *>(alloc_info.pMappedData), size);
	buf.remaining_free_span = buf.host_mapped_span;

	DebugUtils &debug = m_device.instance().debug();
	if (debug.available()) {
		char name[64];
		snprintf(name, std::size(name), "fctx/const_buf_%zu", m_const_upload_buffers.size());
		debug.setObjectName(m_device.handle(), buf.buffer, name);
	}
}

// FrameContextRing

FrameContextRing::FrameContextRing(Device &device, size_t size)
	: m_contexts(size, [&](void *place, size_t) { new (place) FrameContext(device); })
{
	// Make the first context begin recording its command buffer
	current().waitAndReset();
}

uint64_t FrameContextRing::submitAndAdvance(VkSemaphore wait_binary_semaphore, VkSemaphore signal_binary_semaphore)
{
	try {
		uint64_t timeline = current().submit(wait_binary_semaphore, signal_binary_semaphore);
		m_current = (m_current + 1) % m_contexts.size();
		current().waitAndReset();
		return timeline;
	}
	catch (...) {
		Log::error("Failure during frame context ring operation! Destroying the ring");
		m_contexts = {};
		throw;
	}
}

} // namespace voxen::gfx::vk
