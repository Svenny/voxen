#include <voxen/client/vulkan/device.hpp>

#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

namespace voxen::client
{

VulkanDevice::VulkanDevice(VulkanBackend &backend) : m_backend(backend) {
	throw MessageException("not implemented yet");
}

VulkanDevice::~VulkanDevice() noexcept {
	Log::debug("Destroying VkDevice");
	m_backend.vkDestroyDevice(m_device, VulkanHostAllocator::callbacks());
}

}
