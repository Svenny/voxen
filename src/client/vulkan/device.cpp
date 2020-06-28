#include <voxen/client/vulkan/device.hpp>

#include <voxen/client/vulkan/instance.hpp>

#include <voxen/util/assert.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

#include <extras/dyn_array.hpp>

namespace voxen::client
{

VulkanDevice::VulkanDevice(VulkanBackend &backend) : m_backend(backend) {
	Log::debug("Creating VulkanDevice");
	if (!pickPhysicalDevice())
		throw MessageException("failed to pick Vulkan physical device");
	if (!createLogicalDevice())
		throw MessageException("failed to create Vulkan logical device");
	if (!backend.loadDeviceLevelApi(m_device)) {
		destroyDevice();
		throw MessageException("failed to load device-level Vulkan API");
	}
	m_queue_manager.getHandles(backend, m_device);
	Log::debug("VulkanDevice created successfully");
}

VulkanDevice::~VulkanDevice() noexcept {
	Log::debug("Destroying VulkanDevice");
	destroyDevice();
}

void VulkanDevice::waitIdle() {
	VkResult result = m_backend.vkDeviceWaitIdle(m_device);
	if (result != VK_SUCCESS)
		throw VulkanException(result);
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

	auto api_version = std::make_pair(VK_VERSION_MAJOR(props.apiVersion),
	                                  VK_VERSION_MINOR(props.apiVersion));
	auto required_api_version = std::make_pair(VulkanInstance::kMinVulkanVersionMajor,
	                                           VulkanInstance::kMinVulkanVersionMinor);
	if (api_version < required_api_version) {
		Log::debug("'{}' is skipped because its supported API {}.{} is lower than required {}.{}",
		           api_version.first, api_version.second, required_api_version.first, required_api_version.second);
		return false;
	}

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

	// It's dependency `VK_KHR_surface` is guaranteed to
	// be satisfied by GLFW-provided required extensions list
	ext_list.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	// It's dependency `VK_KHR_get_physical_device_properties2` is promoted to Vulkan 1.1
	ext_list.emplace_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
	// It's promoted to Vulkan 1.2, but we need to support 1.1
	ext_list.emplace_back(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME);
	// It's promoted to Vulkan 1.2, but we need to support 1.1
	ext_list.emplace_back(VK_KHR_IMAGELESS_FRAMEBUFFER_EXTENSION_NAME);

	// TODO: warn about unsupported extensions?
	if (!ext_list.empty())
		Log::info("Requesting the following Vulkan device extensions:");
	for (const char *name : ext_list)
		Log::info("{}", name);
	return ext_list;
}

bool VulkanDevice::createLogicalDevice() {
	// This shouldn't be called in already constructed object
	vxAssert(m_device == VK_NULL_HANDLE);

	// Fill VkPhysicalDevice*Features
	VkPhysicalDeviceImagelessFramebufferFeatures imageless_features = {};
	imageless_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES;
	imageless_features.imagelessFramebuffer = VK_TRUE;

	VkPhysicalDeviceFeatures2 features = {};
	features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features.pNext = &imageless_features;

	// Fill VkDeviceCreateInfo
	auto queue_create_infos = m_queue_manager.getCreateInfos();
	auto ext_list = getRequiredDeviceExtensions();
	VkDeviceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.pNext = &features;
	create_info.queueCreateInfoCount = uint32_t(queue_create_infos.size());
	create_info.pQueueCreateInfos = queue_create_infos.data();
	create_info.enabledExtensionCount = uint32_t(ext_list.size());
	create_info.ppEnabledExtensionNames = ext_list.data();

	VkResult result = m_backend.vkCreateDevice(m_phys_device, &create_info, VulkanHostAllocator::callbacks(), &m_device);
	if (result != VK_SUCCESS) {
		Log::error("vkCreateDevice failed: {}", getVkResultString(result));
		return false;
	}
	return true;
}

void VulkanDevice::destroyDevice() noexcept {
	m_backend.vkDestroyDevice(m_device, VulkanHostAllocator::callbacks());
	m_backend.unloadDeviceLevelApi();
}

}
