#include <voxen/client/vulkan/device.hpp>

#include <voxen/client/vulkan/instance.hpp>

#include <voxen/util/assert.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

#include <extras/dyn_array.hpp>

namespace voxen::client
{

VulkanDevice::VulkanDevice(VulkanBackend &backend) : m_backend(backend) {
	if (!pickPhysicalDevice())
		throw MessageException("failed to pick Vulkan physical device");
	if (!createLogicalDevice())
		throw MessageException("failed to create Vulkan logical device");
	if (!backend.loadDeviceLevelApi(m_device))
		throw MessageException("failed to load device-level Vulkan API");
	m_queue_manager.getHandles(backend, m_device);
}

VulkanDevice::~VulkanDevice() noexcept {
	Log::debug("Destroying VkDevice");
	m_backend.vkDestroyDevice(m_device, VulkanHostAllocator::callbacks());
}

bool VulkanDevice::pickPhysicalDevice() {
	// This shouldn't be called in already constructed object
	vxAssert(m_phys_device == VK_NULL_HANDLE);

	VkInstance instance = m_backend.instance()->handle();

	uint32_t num_devices = 0;
	VkResult result = m_backend.vkEnumeratePhysicalDevices(instance, &num_devices, nullptr);
	if (result != VK_SUCCESS) {
		Log::error("vkEnumeratePhysicalDevices failed: {}", getVkResultString(result));
		return false;
	}
	if (num_devices == 0) {
		Log::error("No Vulkan physical devices are present in the system");
		return false;
	}

	extras::dyn_array<VkPhysicalDevice> devices(num_devices);
	result = m_backend.vkEnumeratePhysicalDevices(instance, &num_devices, devices.data());
	if (result != VK_SUCCESS) {
		Log::error("vkEnumeratePhysicalDevices failed: {}", getVkResultString(result));
		return false;
	}

	// TODO: check config first to remember the last selected GPU
	for (const auto &dev : devices) {
		if (isDeviceSuitable(dev)) {
			m_phys_device = dev;
			break;
		}
	}

	if (m_phys_device == VK_NULL_HANDLE) {
		Log::error("No suitable Vulkan physical device found in the system");
		return false;
	}

	VkPhysicalDeviceProperties props;
	m_backend.vkGetPhysicalDeviceProperties(m_phys_device, &props);
	Log::info("Selected GPU is '{}'", props.deviceName);
	uint32_t api = props.apiVersion;
	Log::info("Vulkan device version is {}.{}.{}", VK_VERSION_MAJOR(api), VK_VERSION_MINOR(api), VK_VERSION_PATCH(api));
	return true;
}

bool VulkanDevice::isDeviceSuitable(VkPhysicalDevice device) {
	VkPhysicalDeviceProperties props;
	m_backend.vkGetPhysicalDeviceProperties(device, &props);
	Log::debug("Trying GPU '{}'", props.deviceName);

	// TODO: remove it when saving the selected GPU is available
	if constexpr (BuildConfig::kUseIntegratedGpu) {
		if (props.deviceType != VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
			Log::debug("'{}' is skipped because it's not an integrated GPU", props.deviceName);
			return false;
		}
	} else {
		if (props.deviceType != VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			Log::debug("'{}' is skipped because it's not a discrete GPU", props.deviceName);
			return false;
		}
	}

	VkPhysicalDeviceFeatures features;
	m_backend.vkGetPhysicalDeviceFeatures(device, &features);
	// TODO: check for presence of needed features?

	if (!m_queue_manager.findFamilies(m_backend, device)) {
		Log::debug("'{}' is skipped because it lacks some queue family capabilities");
		return false;
	}

	return true;
}

std::vector<const char *> VulkanDevice::getRequiredDeviceExtensions() {
	std::vector<const char *> ext_list;

	ext_list.emplace_back("VK_KHR_swapchain");

	// TODO: warn about unsupported extensions?
	if (!ext_list.empty())
		Log::info("Requesting the following Vulkan device extensions:");
	for (const char *name : ext_list)
		Log::info("{}", name);
	return ext_list;
}

VkPhysicalDeviceFeatures VulkanDevice::getRequiredFeatures() {
	VkPhysicalDeviceFeatures features = {};
	// TODO: request something?
	return features;
}

bool VulkanDevice::createLogicalDevice() {
	// This shouldn't be called in already constructed object
	vxAssert(m_device == VK_NULL_HANDLE);

	// Fill VkDeviceCreateInfo
	auto queue_create_infos = m_queue_manager.getCreateInfos();
	auto ext_list = getRequiredDeviceExtensions();
	auto required_features = getRequiredFeatures();
	VkDeviceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.queueCreateInfoCount = uint32_t(queue_create_infos.size());
	create_info.pQueueCreateInfos = queue_create_infos.data();
	create_info.enabledExtensionCount = uint32_t(ext_list.size());
	create_info.ppEnabledExtensionNames = ext_list.data();
	create_info.pEnabledFeatures = &required_features;

	VkResult result = m_backend.vkCreateDevice(m_phys_device, &create_info, VulkanHostAllocator::callbacks(), &m_device);
	if (result != VK_SUCCESS) {
		Log::error("vkCreateDevice failed: {}", getVkResultString(result));
		return false;
	}
	Log::debug("VkDevice created successfully");
	return true;
}

}
