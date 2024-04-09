#pragma once

#include <voxen/client/vulkan/common.hpp>

#include <vector>

namespace voxen::client::vulkan
{

class Device {
public:
	Device();
	Device(Device &&) = delete;
	Device(const Device &) = delete;
	Device &operator=(Device &&) = delete;
	Device &operator=(const Device &) = delete;
	~Device() noexcept;

	void waitIdle();

	VkQueue graphicsQueue() const noexcept { return m_graphics_queue; }
	VkQueue computeQueue() const noexcept { return m_compute_queue; }
	VkQueue transferQueue() const noexcept { return m_transfer_queue; }
	VkQueue presentQueue() const noexcept { return m_present_queue; }

	operator VkDevice() const noexcept { return m_device; }

private:
	VkDevice m_device = VK_NULL_HANDLE;
	VkQueue m_graphics_queue = VK_NULL_HANDLE;
	VkQueue m_compute_queue = VK_NULL_HANDLE;
	VkQueue m_transfer_queue = VK_NULL_HANDLE;
	VkQueue m_present_queue = VK_NULL_HANDLE;

	static std::vector<const char *> getRequiredDeviceExtensions();

	void createDevice();
	void obtainQueueHandles() noexcept;
	void destroyDevice() noexcept;
};

} // namespace voxen::client::vulkan
