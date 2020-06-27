#include <voxen/client/vulkan/command_buffer.hpp>

#include <voxen/client/vulkan/backend.hpp>

#include <voxen/util/assert.hpp>

namespace voxen::client
{

void VulkanCommandBuffer::reset(bool release_resources) {
	vxAssert(m_state != State::Pending);

	VkCommandBufferResetFlags flags = 0;
	if (release_resources)
		flags = VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT;

	VkResult result = VulkanBackend::backend().vkResetCommandBuffer(m_cmd_buffer, flags);
	if (result != VK_SUCCESS)
		throw VulkanException(result);
	m_state = State::Initial;
}

void VulkanCommandBuffer::begin(const VkCommandBufferBeginInfo &info) {
	vxAssert(m_state != State::Recording && m_state != State::Pending);

	VkResult result = VulkanBackend::backend().vkBeginCommandBuffer(m_cmd_buffer, &info);
	if (result != VK_SUCCESS)
		throw VulkanException(result);
	m_state = State::Recording;
}

void VulkanCommandBuffer::end() {
	vxAssert(m_state == State::Recording);

	VkResult result = VulkanBackend::backend().vkEndCommandBuffer(m_cmd_buffer);
	if (result != VK_SUCCESS) {
		m_state = State::Invalid;
		throw VulkanException(result);
	}
	m_state = State::Executable;
}

}
