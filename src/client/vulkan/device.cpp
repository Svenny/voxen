#include <voxen/client/vulkan/device.hpp>

#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

namespace voxen::client
{

VulkanDevice::VulkanDevice() {
	throw MessageException("not implemented yet");
}

VulkanDevice::~VulkanDevice() {
	Log::debug("Destroying VkDevice");
	vkDestroyDevice(m_device, VulkanHostAllocator::callbacks());
}

}
