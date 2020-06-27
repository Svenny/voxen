#pragma once

#include <voxen/client/vulkan/common.hpp>
#include <voxen/client/window.hpp>

#include <string_view>

namespace voxen::client
{

class VulkanInstance;
class VulkanDevice;
class VulkanSwapchain;
class VulkanRenderPassCollection;

class VulkanBackend {
public:
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
	VulkanRenderPassCollection *renderPassCollection() const noexcept { return m_render_pass_collection; }

	bool loadInstanceLevelApi(VkInstance instance) noexcept;
	void unloadInstanceLevelApi() noexcept;
	bool loadDeviceLevelApi(VkDevice device) noexcept;
	void unloadDeviceLevelApi() noexcept;

	// Declare pointers to Vulkan API entry points, moved
	// into a separate file because of size and ugliness
#include "api_table_declare.in"

	// VulkanBackend is the only Vulkan-related class designed
	// to be a singleton. The main arguments for this decision:
	// 1. Launching multiple backends makes no sense
	// 2. With non-singleton design each downstream entity would have to store reference
	//    to its backend, needlessly (because of p.1) increasing objects' and code size
	// 3. There is a lot of downstream entities (which strengthens p.2)
	static VulkanBackend &backend() noexcept { return s_instance; }
private:
	State m_state = State::NotStarted;

	VulkanInstance *m_instance = nullptr;
	VulkanDevice *m_device = nullptr;
	VulkanSwapchain *m_swapchain = nullptr;
	VulkanRenderPassCollection *m_render_pass_collection = nullptr;

	static VulkanBackend s_instance;

	static std::string_view stateToString(State state) noexcept;

	bool loadPreInstanceApi() noexcept;

	VulkanBackend() = default;
	VulkanBackend(VulkanBackend &&) = delete;
	VulkanBackend(const VulkanBackend &) = delete;
	VulkanBackend &operator = (VulkanBackend &&) = delete;
	VulkanBackend &operator = (const VulkanBackend &) = delete;
	~VulkanBackend() noexcept;
};

}
