#pragma once

#include <voxen/client/vulkan/common.hpp>

namespace voxen::client
{

class VulkanInstance;
class VulkanDevice;

class VulkanBackend {
public:
	VulkanBackend() = default;
	VulkanBackend(VulkanBackend &&) = delete;
	VulkanBackend(const VulkanBackend &) = delete;
	VulkanBackend &operator = (VulkanBackend &&) = delete;
	VulkanBackend &operator = (const VulkanBackend &) = delete;
	~VulkanBackend() noexcept;

	enum class State {
		NotStarted,
		Started,
		DeviceLost,
		SwapchainOutOfDate,
	};

	bool start() noexcept;
	void stop() noexcept;

	State state() const noexcept { return m_state; }

	VulkanInstance *instance() const noexcept { return m_instance; }
	VulkanDevice *device() const noexcept { return m_device; }

#define VK_INSTANCE_API_ENTRY(name) PFN_##name name = nullptr;
#define VK_DEVICE_API_ENTRY(name) PFN_##name name = nullptr;
#include <voxen/client/vulkan/api_table.in>
#undef VK_DEVICE_API_ENTRY
#undef VK_INSTANCE_API_ENTRY

private:
	State m_state = State::NotStarted;

	VulkanInstance *m_instance = nullptr;
	VulkanDevice *m_device = nullptr;

	bool loadPreInstanceApi() noexcept;
	bool loadInstanceLevelApi(VkInstance instance) noexcept;
	bool loadDeviceLevelApi(VkDevice device) noexcept;
	void unloadApi() noexcept;
};

}
