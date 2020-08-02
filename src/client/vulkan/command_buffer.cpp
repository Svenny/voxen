#include <voxen/client/vulkan/command_buffer.hpp>

#include <voxen/client/vulkan/backend.hpp>

#include <voxen/util/assert.hpp>

namespace voxen::client::vulkan
{

void CommandBuffer::reset(bool release_resources)
{
	vxAssert(m_state != State::Pending);

	VkCommandBufferResetFlags flags = 0;
	if (release_resources)
		flags = VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT;

	VkResult result = Backend::backend().vkResetCommandBuffer(m_cmd_buffer, flags);
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkResetCommandBuffer");
	m_state = State::Initial;
}

void CommandBuffer::begin(const VkCommandBufferBeginInfo &info)
{
	vxAssert(m_state != State::Recording && m_state != State::Pending);

	VkResult result = Backend::backend().vkBeginCommandBuffer(m_cmd_buffer, &info);
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkBeginCommandBuffer");
	m_state = State::Recording;
}

void CommandBuffer::end()
{
	vxAssert(m_state == State::Recording);

	VkResult result = Backend::backend().vkEndCommandBuffer(m_cmd_buffer);
	if (result != VK_SUCCESS) {
		m_state = State::Invalid;
		throw VulkanException(result, "vkEndCommandBuffer");
	}
	m_state = State::Executable;
}

}
