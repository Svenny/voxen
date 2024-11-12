#pragma once

#include <voxen/gfx/vk/vk_include.hpp>

namespace voxen::client::vulkan
{

class CommandBuffer {
public:
	enum class State {
		Initial,
		Recording,
		Executable,
		Pending,
		Invalid
	};

	CommandBuffer() = default;

	explicit CommandBuffer(VkCommandBuffer cmd_buffer, State state) noexcept : m_cmd_buffer(cmd_buffer), m_state(state)
	{}

	CommandBuffer(CommandBuffer &&other) = default;
	CommandBuffer(const CommandBuffer &) = delete;
	CommandBuffer &operator=(CommandBuffer &&) = default;
	CommandBuffer &operator=(const CommandBuffer &) = delete;
	~CommandBuffer() = default;

	void reset(bool release_resources = false);
	void begin(const VkCommandBufferBeginInfo &info);
	void end();

	State state() const noexcept { return m_state; }

	operator VkCommandBuffer() const noexcept { return m_cmd_buffer; }

private:
	VkCommandBuffer m_cmd_buffer = VK_NULL_HANDLE;
	State m_state = State::Invalid;
};

} // namespace voxen::client::vulkan
