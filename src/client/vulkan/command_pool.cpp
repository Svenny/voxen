#include <voxen/client/vulkan/command_pool.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>

namespace voxen::client
{

VulkanCommandPool::VulkanCommandPool(uint32_t queue_family) {
	VkCommandPoolCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	// Currently we don't use pre-recorded buffers. They
	// are re-recorded each frame, hence these flags.
	info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	info.queueFamilyIndex = queue_family;

	auto &backend = VulkanBackend::backend();
	VkDevice device = *backend.device();
	VkResult result = backend.vkCreateCommandPool(device, &info, VulkanHostAllocator::callbacks(), &m_cmd_pool);
	if (result != VK_SUCCESS)
		throw VulkanException(result);
}

extras::dyn_array<VulkanCommandBuffer> VulkanCommandPool::allocateCommandBuffers(uint32_t count, bool secondary) {
	extras::dyn_array<VkCommandBuffer> handles(count);

	VkCommandBufferAllocateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	info.commandPool = m_cmd_pool;
	info.level = secondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	info.commandBufferCount = count;

	auto &backend = VulkanBackend::backend();
	VkDevice device = *backend.device();
	VkResult result = backend.vkAllocateCommandBuffers(device, &info, handles.data());
	if (result != VK_SUCCESS)
		throw VulkanException(result);

	extras::dyn_array<VulkanCommandBuffer> buffers(count);
	for (uint32_t i = 0; i < count; i++)
		buffers[i] = VulkanCommandBuffer(handles[i], VulkanCommandBuffer::State::Initial);
	return buffers;
}

void VulkanCommandPool::freeCommandBuffers(extras::dyn_array<VulkanCommandBuffer> &buffers) {
	extras::dyn_array<VkCommandBuffer> handles(buffers.size());
	for (size_t i = 0; i < buffers.size(); i++)
		handles[i] = buffers[i];

	auto &backend = VulkanBackend::backend();
	VkDevice device = *backend.device();
	backend.vkFreeCommandBuffers(device, m_cmd_pool, uint32_t(buffers.size()), handles.data());
	for (auto &buf : buffers)
		buf = VulkanCommandBuffer();
}

void VulkanCommandPool::trim() noexcept {
	auto &backend = VulkanBackend::backend();
	VkDevice device = *backend.device();
	backend.vkTrimCommandPool(device, m_cmd_pool, 0);
}

void VulkanCommandPool::reset(bool release_resources) {
	VkCommandPoolResetFlags flags = 0;
	if (release_resources)
		flags = VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT;

	auto &backend = VulkanBackend::backend();
	VkDevice device = *backend.device();
	VkResult result = backend.vkResetCommandPool(device, m_cmd_pool, flags);
	if (result != VK_SUCCESS)
		throw VulkanException(result);
}

VulkanCommandPool::~VulkanCommandPool() noexcept {
	auto &backend = VulkanBackend::backend();
	VkDevice device = *backend.device();
	backend.vkDestroyCommandPool(device, m_cmd_pool, VulkanHostAllocator::callbacks());
}

}
