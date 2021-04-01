#include <voxen/client/vulkan/device.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/physical_device.hpp>

#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

#include <extras/defer.hpp>

namespace voxen::client::vulkan
{

Device::Device()
{
	Log::debug("Creating Device");

	createDevice();

	auto &backend = Backend::backend();
	if (!backend.loadDeviceLevelApi(m_device)) {
		destroyDevice();
		throw MessageException("failed to load device-level Vulkan API");
	}

	obtainQueueHandles();

	Log::debug("Device created successfully");
}

Device::~Device() noexcept
{
	Log::debug("Destroying Device");
	destroyDevice();
}

void Device::waitIdle()
{
	auto &backend = Backend::backend();
	VkResult result = backend.vkDeviceWaitIdle(m_device);
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkDeviceWaitIdle");
}

std::vector<const char *> Device::getRequiredDeviceExtensions()
{
	std::vector<const char *> ext_list
	{
		// It's dependency `VK_KHR_surface` is guaranteed to
		// be satisfied by GLFW-provided required instance extensions list
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		// It's dependency `VK_KHR_get_physical_device_properties2` is promoted to Vulkan 1.1
		VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
	};

	// TODO: warn about unsupported extensions?
	if (!ext_list.empty())
		Log::info("Requesting the following Vulkan device extensions:");
	for (const char *name : ext_list)
		Log::info("{}", name);
	return ext_list;
}

void Device::createDevice()
{
	// This shouldn't be called in already constructed object
	assert(m_device == VK_NULL_HANDLE);

	// Fill VkPhysicalDevice*Features
	VkPhysicalDeviceImagelessFramebufferFeatures imageless_features = {};
	imageless_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES;
	imageless_features.imagelessFramebuffer = VK_TRUE;

	VkPhysicalDeviceFeatures2 features = {};
	features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features.pNext = &imageless_features;
	features.features.fillModeNonSolid = VK_TRUE; // For debug octree drawing

	auto &backend = Backend::backend();
	auto &phys_device = backend.physicalDevice();

	// Fill VkDeviceQueueCreateInfo's
	std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
	{
		// VkDeviceQueueCreateInfo uses a pointer to a float for priorities array.
		// We don't use priorities now, so just point it to a single constant.
		static constexpr float QUEUE_PRIORITY = 1.0f;

		VkDeviceQueueCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		info.queueCount = 1;
		info.pQueuePriorities = &QUEUE_PRIORITY;

		// Add `info` to `result`, setting queue family index
		// to `idx` and preventing possible duplicates
		auto addItem = [&queue_create_infos, &info](uint32_t idx) {
			info.queueFamilyIndex = idx;
			for (const auto &item : queue_create_infos)
				if (item.queueFamilyIndex == info.queueFamilyIndex)
					return;
			queue_create_infos.push_back(info);
		};

		addItem(phys_device.graphicsQueueFamily());
		addItem(phys_device.computeQueueFamily());
		addItem(phys_device.transferQueueFamily());
		addItem(phys_device.presentQueueFamily());
	}

	// Fill VkDeviceCreateInfo
	auto ext_list = getRequiredDeviceExtensions();
	VkDeviceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.pNext = &features;
	create_info.queueCreateInfoCount = uint32_t(queue_create_infos.size());
	create_info.pQueueCreateInfos = queue_create_infos.data();
	create_info.enabledExtensionCount = uint32_t(ext_list.size());
	create_info.ppEnabledExtensionNames = ext_list.data();

	VkResult result = backend.vkCreateDevice(phys_device, &create_info, VulkanHostAllocator::callbacks(), &m_device);
	if (result != VK_SUCCESS)
		throw VulkanException(result, "vkCreateDevice");
}

void Device::obtainQueueHandles() noexcept
{
	auto &backend = Backend::backend();
	const auto &phys_device = backend.physicalDevice();
	backend.vkGetDeviceQueue(m_device, phys_device.graphicsQueueFamily(), 0, &m_graphics_queue);
	backend.vkGetDeviceQueue(m_device, phys_device.computeQueueFamily(), 0, &m_compute_queue);
	backend.vkGetDeviceQueue(m_device, phys_device.transferQueueFamily(), 0, &m_transfer_queue);
	backend.vkGetDeviceQueue(m_device, phys_device.presentQueueFamily(), 0, &m_present_queue);
}

void Device::destroyDevice() noexcept
{
	auto &backend = Backend::backend();
	backend.vkDestroyDevice(m_device, VulkanHostAllocator::callbacks());
	backend.unloadDeviceLevelApi();
}

}
