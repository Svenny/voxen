#include <voxen/client/vulkan/command_pool.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>

namespace voxen::client::vulkan
{

CommandPool::CommandPool(uint32_t queue_family)
{
	VkCommandPoolCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	// Currently we don't use pre-recorded buffers. They
	// are re-recorded each frame, hence these flags.
	info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	info.queueFamilyIndex = queue_family;

	auto &backend = Backend::backend();
	VkDevice device = backend.device();
	VkResult result = backend.vkCreateCommandPool(device, &info, HostAllocator::callbacks(), &m_cmd_pool);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkCreateCommandPool");
	}
}

extras::dyn_array<CommandBuffer> CommandPool::allocateCommandBuffers(uint32_t count, bool secondary)
{
	extras::dyn_array<VkCommandBuffer> handles(count);

	VkCommandBufferAllocateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	info.commandPool = m_cmd_pool;
	info.level = secondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	info.commandBufferCount = count;

	auto &backend = Backend::backend();
	VkDevice device = backend.device();
	VkResult result = backend.vkAllocateCommandBuffers(device, &info, handles.data());
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkAllocateCommandBuffers");
	}

	extras::dyn_array<CommandBuffer> buffers(count);
	for (uint32_t i = 0; i < count; i++) {
		buffers[i] = CommandBuffer(handles[i], CommandBuffer::State::Initial);
	}
	return buffers;
}

void CommandPool::freeCommandBuffers(extras::dyn_array<CommandBuffer> &buffers)
{
	extras::dyn_array<VkCommandBuffer> handles(buffers.size());
	for (size_t i = 0; i < buffers.size(); i++) {
		handles[i] = buffers[i];
	}

	auto &backend = Backend::backend();
	VkDevice device = backend.device();
	backend.vkFreeCommandBuffers(device, m_cmd_pool, uint32_t(buffers.size()), handles.data());
	for (auto &buf : buffers) {
		buf = CommandBuffer();
	}
}

void CommandPool::trim() noexcept
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device();
	backend.vkTrimCommandPool(device, m_cmd_pool, 0);
}

void CommandPool::reset(bool release_resources)
{
	VkCommandPoolResetFlags flags = 0;
	if (release_resources) {
		flags = VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT;
	}

	auto &backend = Backend::backend();
	VkDevice device = backend.device();
	VkResult result = backend.vkResetCommandPool(device, m_cmd_pool, flags);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkResetCommandPool");
	}
}

CommandPool::~CommandPool() noexcept
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device();
	backend.vkDestroyCommandPool(device, m_cmd_pool, HostAllocator::callbacks());
}

} // namespace voxen::client::vulkan
