#pragma once

#include <voxen/client/vulkan/common.hpp>

namespace voxen::client
{

class VulkanCommandBuffer {
public:
	enum class State {
		Initial,
		Recording,
		Executable,
		Pending,
		Invalid
	};

	VulkanCommandBuffer() = default;

	explicit VulkanCommandBuffer(VkCommandBuffer cmd_buffer, State state) noexcept
		: m_cmd_buffer(cmd_buffer), m_state(state) {}

	VulkanCommandBuffer(VulkanCommandBuffer &&other) = default;
	VulkanCommandBuffer(const VulkanCommandBuffer &) = delete;
	VulkanCommandBuffer& operator = (VulkanCommandBuffer &&) = default;
	VulkanCommandBuffer& operator = (const VulkanCommandBuffer &) = delete;
	~VulkanCommandBuffer() = default;

	void reset(bool release_resources = false);

	State state() const noexcept { return m_state; }

	operator VkCommandBuffer() const noexcept { return m_cmd_buffer; }
private:
	VkCommandBuffer m_cmd_buffer = VK_NULL_HANDLE;
	State m_state = State::Invalid;
};

}
