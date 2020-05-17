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
private:
	State m_state = State::NotStarted;

	VulkanInstance *m_instance = nullptr;
	VulkanDevice *m_device = nullptr;
};

}
