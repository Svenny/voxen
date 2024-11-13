#include <voxen/gfx/vk/vk_command_allocator.hpp>

#include <voxen/gfx/gfx_system.hpp>
#include <voxen/gfx/vk/vk_error.hpp>

#include <extras/defer.hpp>

#include <cassert>

namespace voxen::gfx::vk
{

namespace
{

constexpr uint32_t POOL_MAX_EXCESS_CMDBUFS = 3;

const char *getQueueName(Device::Queue queue) noexcept
{
	switch (queue) {
	case Device::QueueMain:
		return "main";
	case Device::QueueDma:
		return "dma";
	case Device::QueueCompute:
		return "compute";
	case Device::QueueCount:
		assert(false);
		return "";
	}
}

VkCommandPool createCommandPool(Device &dev, Device::Queue queue, char letter)
{
	VkCommandPoolCreateInfo pool_create_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.queueFamilyIndex = 0,
	};

	switch (queue) {
	case Device::QueueMain:
		pool_create_info.queueFamilyIndex = dev.info().main_queue_family;
		break;
	case Device::QueueDma:
		pool_create_info.queueFamilyIndex = dev.info().dma_queue_family;
		break;
	case Device::QueueCompute:
		pool_create_info.queueFamilyIndex = dev.info().compute_queue_family;
		break;
	case Device::QueueCount:
		assert(false);
	}

	VkCommandPool pool = VK_NULL_HANDLE;
	VkResult res = dev.dt().vkCreateCommandPool(dev.handle(), &pool_create_info, nullptr, &pool);
	if (res != VK_SUCCESS) [[unlikely]] {
		throw VulkanException(res, "vkCreateCommandPool");
	}

	defer_fail { dev.dt().vkDestroyCommandPool(dev.handle(), pool, nullptr); };

	char name_buf[64];
	snprintf(name_buf, std::size(name_buf), "cmdpool/%s@%c", getQueueName(queue), letter);
	dev.setObjectName(pool, name_buf);

	return pool;
}

} // namespace

CommandAllocator::CommandAllocator(GfxSystem &gfx) : m_gfx(gfx)
{
	Device &dev = *m_gfx.device();

	defer_fail {
		for (auto &pool_set : m_command_pools) {
			for (VkCommandPool pool : pool_set) {
				dev.dt().vkDestroyCommandPool(dev.handle(), pool, nullptr);
			}
		}
	};

	for (uint32_t pool_set = 0; pool_set < gfx::Consts::MAX_PENDING_FRAMES; pool_set++) {
		for (uint32_t q = 0; q < Device::QueueCount; q++) {
			m_command_pools[pool_set][q] = createCommandPool(dev, Device::Queue(q), char('A' + pool_set));
		}
	}
}

CommandAllocator::~CommandAllocator()
{
	for (auto &pool_set : m_command_pools) {
		for (VkCommandPool pool : pool_set) {
			m_gfx.device()->enqueueDestroy(pool);
		}
	}
}

VkCommandBuffer CommandAllocator::allocate(Device::Queue queue)
{
	assert(queue < Device::QueueCount);

	auto &cmd_buffers = m_command_buffers[m_current_set][queue];

	uint32_t next_index = m_cmd_buffer_index[queue];
	if (next_index < cmd_buffers.size()) [[likely]] {
		++m_cmd_buffer_index[queue];
		return cmd_buffers[next_index];
	}

	// Add handle slot
	VkCommandBuffer &cmd_buf = m_command_buffers[m_current_set][queue].emplace_back();
	m_cmd_buffer_index[queue] = static_cast<uint32_t>(cmd_buffers.size());

	defer_fail { cmd_buffers.pop_back(); };

	Device &dev = *m_gfx.device();

	VkCommandBufferAllocateInfo alloc_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext = nullptr,
		.commandPool = m_command_pools[m_current_set][queue],
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};

	VkResult res = dev.dt().vkAllocateCommandBuffers(dev.handle(), &alloc_info, &cmd_buf);
	if (res != VK_SUCCESS) [[unlikely]] {
		throw VulkanException(res, "vkAllocateCommandBuffers");
	}

	char letter = char('A' + m_current_set);
	char name_buf[64];
	snprintf(name_buf, std::size(name_buf), "cmdpool/%s@%c/buf#%zu", getQueueName(queue), letter, cmd_buffers.size());
	dev.setObjectName(cmd_buf, name_buf);

	return cmd_buf;
}

void CommandAllocator::onFrameTickBegin(FrameTickId completed_tick, FrameTickId new_tick)
{
	m_current_set = static_cast<uint64_t>(new_tick.value) % gfx::Consts::MAX_PENDING_FRAMES;
	FrameTickId old_tick = std::exchange(m_tick_ids[m_current_set], new_tick);

	if (old_tick > completed_tick) {
		m_gfx.waitFrameCompletion(old_tick);
	}

	VkDevice vk_device = m_gfx.device()->handle();
	auto &dt = m_gfx.device()->dt();

	for (auto &pool_set : m_command_pools) {
		for (VkCommandPool pool : pool_set) {
			VkResult res = dt.vkResetCommandPool(vk_device, pool, 0);
			if (res != VK_SUCCESS) [[unlikely]] {
				throw VulkanException(res, "vkResetCommandPool");
			}
		}
	}
}

void CommandAllocator::onFrameTickEnd(FrameTickId /*current_tick*/)
{
	// Reset counters of used command buffers
	for (uint32_t q = 0; q < Device::QueueCount; q++) {
		uint32_t used_cmdbufs = std::exchange(m_cmd_buffer_index[q], 0);

		if (m_command_buffers[m_current_set][q].size() > used_cmdbufs + POOL_MAX_EXCESS_CMDBUFS) {
			// This pool has too many excessive command buffers, shrink by replacement
			VkCommandPool replacement_pool = createCommandPool(*m_gfx.device(), Device::Queue(q),
				char('A' + m_current_set));
			m_gfx.device()->enqueueDestroy(m_command_pools[m_current_set][q]);

			m_command_pools[m_current_set][q] = replacement_pool;
			m_command_buffers[m_current_set][q].clear();
		}
	}
}

} // namespace voxen::gfx::vk
