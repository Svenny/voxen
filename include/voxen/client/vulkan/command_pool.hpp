#pragma once

#include <voxen/client/vulkan/command_buffer.hpp>
#include <voxen/gfx/vk/vk_include.hpp>

#include <extras/dyn_array.hpp>

namespace voxen::client::vulkan
{

class CommandPool {
public:
	explicit CommandPool(uint32_t queue_family);
	CommandPool(CommandPool &&) = delete;
	CommandPool(const CommandPool &) = delete;
	CommandPool &operator=(CommandPool &&) = delete;
	CommandPool &operator=(const CommandPool &) = delete;
	~CommandPool() noexcept;

	extras::dyn_array<CommandBuffer> allocateCommandBuffers(uint32_t count, bool secondary = false);
	void freeCommandBuffers(extras::dyn_array<CommandBuffer> &buffers);
	void trim() noexcept;
	void reset(bool release_resources = false);

	operator VkCommandPool() const noexcept { return m_cmd_pool; }

private:
	VkCommandPool m_cmd_pool = VK_NULL_HANDLE;
};

} // namespace voxen::client::vulkan
