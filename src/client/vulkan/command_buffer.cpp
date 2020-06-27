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

}
