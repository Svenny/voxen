#pragma once

#include <voxen/client/vulkan/common.hpp>
#include <voxen/client/window.hpp>

#include <string_view>

namespace voxen::client
{

class VulkanInstance;
class VulkanDevice;
class VulkanSwapchain;

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
		SurfaceLost,
		SwapchainOutOfDate,
	};

	bool start(Window &window) noexcept;
	void stop() noexcept;

	State state() const noexcept { return m_state; }

	VulkanInstance *instance() const noexcept { return m_instance; }
	VulkanDevice *device() const noexcept { return m_device; }
	VulkanSwapchain *swapchain() const noexcept { return m_swapchain; }

	bool loadInstanceLevelApi(VkInstance instance) noexcept;
	void unloadInstanceLevelApi() noexcept;
	bool loadDeviceLevelApi(VkDevice device) noexcept;
	void unloadDeviceLevelApi() noexcept;

	// Declare pointers to Vulkan API entry points, moved
	// into a separate file because of size and ugliness
#include "api_table_declare.in"

private:
	State m_state = State::NotStarted;

	VulkanInstance *m_instance = nullptr;
	VulkanDevice *m_device = nullptr;
	VulkanSwapchain *m_swapchain = nullptr;

	static std::string_view stateToString(State state) noexcept;

	bool loadPreInstanceApi() noexcept;
};

}
