#include <voxen/gfx/vk/vk_device.hpp>

#include <voxen/client/vulkan/capabilities.hpp>
#include <voxen/client/vulkan/common.hpp>
#include <voxen/gfx/vk/vk_instance.hpp>
#include <voxen/gfx/vk/vk_physical_device.hpp>
#include <voxen/util/error_condition.hpp>
#include <voxen/util/log.hpp>

#include <extras/defer.hpp>

#include <vma/vk_mem_alloc.h>

namespace voxen::gfx::vk
{

// TODO: there parts are not yet moved to voxen/gfx/vk
using client::vulkan::Capabilities;
using client::vulkan::VulkanException;
using client::vulkan::VulkanUtils;

namespace
{

void fillMainDispatchTable(DeviceDispatchTable &dt, PFN_vkGetDeviceProcAddr loader, VkDevice dev)
{
	auto load_entry = [&](const char *name) {
		PFN_vkVoidFunction proc = loader(dev, name);

		if (!proc) {
			Log::error("Can't get '{}' entry point from VkDevice", name);
			throw Exception::fromError(VoxenErrc::GfxCapabilityMissing, "missing Vulkan device entry point");
		}

		return proc;
	};

#define VK_API_ENTRY(x) dt.x = reinterpret_cast<PFN_##x>(load_entry(#x));
#include <voxen/gfx/vk/api_device.in>
#undef VK_API_ENTRY
}

} // namespace

Device::Device(Instance &instance, PhysicalDevice &phys_dev) : m_instance(instance), m_phys_device(phys_dev)
{
	if (!isSupported(phys_dev)) {
		throw Exception::fromError(VoxenErrc::GfxCapabilityMissing, "GPU does not pass minimal requirements");
	}

	createDevice();
	defer_fail { instance.dt().vkDestroyDevice(m_handle, nullptr); };

	fillMainDispatchTable(m_dt, instance.dt().vkGetDeviceProcAddr, m_handle);
	getQueueHandles();

	createVma();
	defer_fail { vmaDestroyAllocator(m_vma); };

	createTimelineSemaphore();
	defer_fail { m_dt.vkDestroySemaphore(m_handle, m_timeline_semaphore, nullptr); };

	auto &props = m_phys_device.info().props.properties;
	Log::info("Created VkDevice from GPU '{}'", props.deviceName);
	uint32_t ver = props.apiVersion;
	Log::info("Device Vulkan version: {}.{}.{}", VK_VERSION_MAJOR(ver), VK_VERSION_MINOR(ver), VK_VERSION_PATCH(ver));
	ver = props.driverVersion;
	Log::info("Device driver version: {}.{}.{}", VK_VERSION_MAJOR(ver), VK_VERSION_MINOR(ver), VK_VERSION_PATCH(ver));
}

Device::~Device() noexcept
{
	if (!m_handle) {
		return;
	}

	Log::debug("Destroying VkDevice");

	// Complete all GPU operations
	VkResult res = m_dt.vkDeviceWaitIdle(m_handle);
	if (res != VK_SUCCESS) {
		// Most likely VK_ERROR_DEVICE_LOST... whatever, we're destroying it anyway
		Log::warn("vkDeviceWaitIdle failed - {}", VulkanUtils::getVkResultString(res));
	}

	// We have just completed everything, might as well write UINT64_MAX here
	m_last_completed_timeline = m_last_submitted_timeline;
	processDestroyQueue();

	// Now destroy device subobjects
	m_dt.vkDestroySemaphore(m_handle, m_timeline_semaphore, nullptr);
	vmaDestroyAllocator(m_vma);

	// And then the device itself
	m_instance.dt().vkDestroyDevice(m_handle, nullptr);

	Log::debug("VkDevice destroyed");
}

uint64_t Device::submitCommands(SubmitInfo info)
{
	// We need to wrap `VkCommandBuffer` handles in structs
	VkCommandBufferSubmitInfo cmdbuf_info_stack[4];
	std::unique_ptr<VkCommandBufferSubmitInfo[]> cmdbuf_info_heap;

	// Assume a few command buffers (the common case)
	// and don't require heap allocation for them
	VkCommandBufferSubmitInfo *cmdbuf_info = cmdbuf_info_stack;
	if (info.cmds.size() > std::size(cmdbuf_info_stack)) {
		cmdbuf_info_heap = std::make_unique<VkCommandBufferSubmitInfo[]>(info.cmds.size());
		cmdbuf_info = cmdbuf_info_heap.get();
	}

	for (size_t i = 0; i < info.cmds.size(); i++) {
		cmdbuf_info[i] = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
			.pNext = nullptr,
			.commandBuffer = info.cmds[i],
			.deviceMask = 0,
		};
	}

	VkSemaphoreSubmitInfo wait_info[2];
	uint32_t wait_info_count = 0;

	if (info.wait_timeline != 0) {
		wait_info[0] = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.pNext = nullptr,
			.semaphore = m_timeline_semaphore,
			.value = info.wait_timeline,
			.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.deviceIndex = 0,
		};
		wait_info_count++;
	}

	if (info.wait_binary_semaphore != VK_NULL_HANDLE) {
		wait_info[wait_info_count] = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.pNext = nullptr,
			.semaphore = info.wait_binary_semaphore,
			.value = 0,
			.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.deviceIndex = 0,
		};
		wait_info_count++;
	}

	VkSemaphoreSubmitInfo signal_info[2];
	uint32_t signal_info_count = 0;

	uint64_t completion_timeline = m_last_submitted_timeline + 1;

	if (info.signal_timeline) {
		signal_info[0] = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.pNext = nullptr,
			.semaphore = m_timeline_semaphore,
			.value = completion_timeline,
			.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.deviceIndex = 0,
		};
		signal_info_count++;
	}

	if (info.signal_binary_semaphore) {
		signal_info[signal_info_count] = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.pNext = nullptr,
			.semaphore = info.signal_binary_semaphore,
			.value = 0,
			.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.deviceIndex = 0,
		};
		signal_info_count++;
	}

	VkSubmitInfo2 submit {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.pNext = nullptr,
		.flags = 0,
		.waitSemaphoreInfoCount = wait_info_count,
		.pWaitSemaphoreInfos = wait_info,
		.commandBufferInfoCount = static_cast<uint32_t>(info.cmds.size()),
		.pCommandBufferInfos = cmdbuf_info,
		.signalSemaphoreInfoCount = signal_info_count,
		.pSignalSemaphoreInfos = signal_info,
	};

	VkQueue queue = VK_NULL_HANDLE;
	switch (info.queue) {
	case SubmitQueue::Main:
		queue = m_main_queue;
		break;
	case SubmitQueue::Compute:
		queue = m_compute_queue;
		break;
	case SubmitQueue::Dma:
		queue = m_dma_queue;
		break;
	}

	VkResult res = m_dt.vkQueueSubmit2(queue, 1, &submit, info.signal_fence);
	if (res != VK_SUCCESS) {
		throw VulkanException(res, "vkQueueSubmit2");
	}

	if (info.signal_timeline) {
		m_last_submitted_timeline++;
		return completion_timeline;
	}

	return 0;
}

void Device::waitForTimeline(uint64_t value)
{
	if (value <= m_last_completed_timeline) {
		return;
	}

	VkSemaphoreWaitInfo wait_info {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
		.pNext = nullptr,
		.flags = 0,
		.semaphoreCount = 1,
		.pSemaphores = &m_timeline_semaphore,
		.pValues = &value,
	};

	VkResult res = m_dt.vkWaitSemaphores(m_handle, &wait_info, UINT64_MAX);
	if (res != VK_SUCCESS) {
		throw VulkanException(res, "vkWaitSemaphores");
	}

	m_last_completed_timeline = value;
	processDestroyQueue();
}

// TODO: replace manual list of checks with Vulkan profiles
bool Device::isSupported(PhysicalDevice &pd)
{
	auto &props = pd.info().props.properties;

	Log::debug("Checking GPU '{}' for minimal requirements", props.deviceName);

	// Vulkan version

	uint32_t apiVersion = props.apiVersion;
	uint32_t minVersion = Capabilities::MIN_VULKAN_VERSION;

	if (apiVersion < minVersion) {
		Log::debug("Device Vulkan version is less than minimal supported");
		Log::debug("Minimal supported version: {}.{}.{}", VK_VERSION_MAJOR(minVersion), VK_VERSION_MINOR(minVersion),
			VK_VERSION_PATCH(minVersion));
		Log::debug("Device version: {}.{}.{}", VK_VERSION_MAJOR(apiVersion), VK_VERSION_MINOR(apiVersion),
			VK_VERSION_PATCH(apiVersion));
		return false;
	}

	if (pd.queueInfo().main_queue_family == VK_QUEUE_FAMILY_IGNORED) {
		Log::debug("Device does not have the main (GRAPHICS+COMPUTE) queue");
		return false;
	}

	std::vector<std::string_view> missing;

	// Required extensions

	auto &ext_info = pd.extInfo();

	if (!ext_info.have_maintenance5) {
		missing.emplace_back(VK_KHR_MAINTENANCE_5_EXTENSION_NAME);
	}
	if (!ext_info.have_push_descriptor) {
		missing.emplace_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
	}
	if (!ext_info.have_swapchain) {
		missing.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	}

	if (!missing.empty()) {
		Log::debug("Device lacks required extensions:");
		for (auto ext : missing) {
			Log::debug("- {}", ext);
		}
		return false;
	}

	// Required features

	auto &info = pd.info();

	if (!info.feats.features.imageCubeArray) {
		missing.emplace_back("imageCubeArray");
	}
	if (!info.feats.features.independentBlend) {
		missing.emplace_back("independentBlend");
	}
	if (!info.feats.features.multiDrawIndirect) {
		missing.emplace_back("multiDrawIndirect");
	}
	if (!info.feats.features.drawIndirectFirstInstance) {
		missing.emplace_back("drawIndirectFirstInstance");
	}
	if (!info.feats.features.fillModeNonSolid) {
		missing.emplace_back("fillModeNonSolid");
	}
	if (!info.feats.features.samplerAnisotropy) {
		missing.emplace_back("samplerAnisotropy");
	}
	if (!info.feats.features.textureCompressionBC) {
		missing.emplace_back("textureCompressionBC");
	}
	if (!info.feats11.shaderDrawParameters) {
		missing.emplace_back("shaderDrawParameters");
	}
	if (!info.feats12.descriptorIndexing) {
		missing.emplace_back("descriptorIndexing");
	}
	if (!info.feats12.scalarBlockLayout) {
		missing.emplace_back("scalarBlockLayout");
	}
	if (!info.feats12.uniformBufferStandardLayout) {
		missing.emplace_back("uniformBufferStandardLayout");
	}
	if (!info.feats12.hostQueryReset) {
		missing.emplace_back("hostQueryReset");
	}
	if (!info.feats12.timelineSemaphore) {
		missing.emplace_back("timelineSemaphore");
	}
	if (!info.feats12.shaderOutputLayer) {
		missing.emplace_back("shaderOutputLayer");
	}
	if (!info.feats13.synchronization2) {
		missing.emplace_back("synchronization2");
	}
	if (!info.feats13.dynamicRendering) {
		missing.emplace_back("dynamicRendering");
	}
	if (!info.feats13.maintenance4) {
		missing.emplace_back("maintenance4");
	}

	if (!missing.empty()) {
		Log::debug("Device lacks required features:");
		for (auto ext : missing) {
			Log::debug("- {}", ext);
		}
		return false;
	}

	Log::debug("GPU '{}' passes minimal requirements", props.deviceName);
	return true;
}

// TODO: replace manual requests with Vulkan profiles
void Device::createDevice()
{
	// Fill VkPhysicalDevice*Features chain
	VkPhysicalDeviceMaintenance5FeaturesKHR features_maintenance5 = {};
	features_maintenance5.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR;
	features_maintenance5.maintenance5 = VK_TRUE;

	VkPhysicalDeviceVulkan13Features features13 = {};
	features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features13.pNext = &features_maintenance5;
	features13.synchronization2 = VK_TRUE;
	features13.dynamicRendering = VK_TRUE;
	features13.maintenance4 = VK_TRUE;

	VkPhysicalDeviceVulkan12Features features12 = {};
	features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	features12.pNext = &features13;
	features12.descriptorIndexing = VK_TRUE;
	features12.scalarBlockLayout = VK_TRUE;
	features12.uniformBufferStandardLayout = VK_TRUE;
	features12.hostQueryReset = VK_TRUE;
	features12.timelineSemaphore = VK_TRUE;
	features12.shaderOutputLayer = VK_TRUE;

	VkPhysicalDeviceVulkan11Features features11 = {};
	features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
	features11.pNext = &features12;
	features11.shaderDrawParameters = VK_TRUE;

	VkPhysicalDeviceFeatures2 features = {};
	features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features.pNext = &features11;
	features.features.imageCubeArray = VK_TRUE;
	features.features.independentBlend = VK_TRUE;
	features.features.multiDrawIndirect = VK_TRUE;
	features.features.drawIndirectFirstInstance = VK_TRUE;
	features.features.fillModeNonSolid = VK_TRUE;
	features.features.samplerAnisotropy = VK_TRUE;
	features.features.textureCompressionBC = VK_TRUE;

	// Fill VkDeviceQueueCreateInfo's
	std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
	{
		// Not that these device-local queue priorities matter much...
		constexpr float QUEUE_PRIORITY = 0.5f;

		VkDeviceQueueCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		info.queueCount = 1;
		info.pQueuePriorities = &QUEUE_PRIORITY;

		auto &queue_info = m_phys_device.queueInfo();
		// Guaranteed to exist by `isSupported()`
		m_info.main_queue_family = queue_info.main_queue_family;

		info.queueFamilyIndex = queue_info.main_queue_family;
		queue_create_infos.emplace_back(info);

		if (queue_info.compute_queue_family != VK_QUEUE_FAMILY_IGNORED) {
			m_info.dedicated_compute_queue = 1;
			m_info.compute_queue_family = queue_info.compute_queue_family;

			info.queueFamilyIndex = queue_info.compute_queue_family;
			queue_create_infos.emplace_back(info);
		} else {
			m_info.compute_queue_family = m_info.main_queue_family;
		}

		if (queue_info.dma_queue_family != VK_QUEUE_FAMILY_IGNORED) {
			m_info.dedicated_dma_queue = 1;
			m_info.dma_queue_family = queue_info.dma_queue_family;

			info.queueFamilyIndex = queue_info.dma_queue_family;
			queue_create_infos.emplace_back(info);
		} else {
			m_info.dma_queue_family = m_info.main_queue_family;
		}
	}

	// Fill requested extensions list + add extension feature requests
	std::vector<const char *> ext_list;
	VkPhysicalDeviceMeshShaderFeaturesEXT features_mesh_shader = {};
	{
		// Required extensions
		ext_list.emplace_back(VK_KHR_MAINTENANCE_5_EXTENSION_NAME);
		ext_list.emplace_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
		ext_list.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

		auto &ext_info = m_phys_device.extInfo();

		if (ext_info.have_memory_budget) {
			m_info.have_memory_budget = 1;
			ext_list.emplace_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
		}

		if (ext_info.have_mesh_shader) {
			m_info.have_mesh_shader = 1;
			ext_list.emplace_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
			features_mesh_shader.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
			features_mesh_shader.pNext = std::exchange(features13.pNext, &features_mesh_shader);
			features_mesh_shader.taskShader = VK_TRUE;
			features_mesh_shader.meshShader = VK_TRUE;
		}
	}

	VkDeviceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.pNext = &features;
	create_info.queueCreateInfoCount = uint32_t(queue_create_infos.size());
	create_info.pQueueCreateInfos = queue_create_infos.data();
	create_info.enabledExtensionCount = uint32_t(ext_list.size());
	create_info.ppEnabledExtensionNames = ext_list.data();

	VkResult result = m_instance.dt().vkCreateDevice(m_phys_device.handle(), &create_info, nullptr, &m_handle);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkCreateDevice");
	}
}

void Device::getQueueHandles()
{
	m_dt.vkGetDeviceQueue(m_handle, m_info.main_queue_family, 0, &m_main_queue);
	m_instance.debug().setObjectName(m_handle, m_main_queue, "common/main_queue");

	if (m_info.dedicated_compute_queue) {
		m_dt.vkGetDeviceQueue(m_handle, m_info.compute_queue_family, 0, &m_compute_queue);
		m_instance.debug().setObjectName(m_handle, m_compute_queue, "common/compute_queue");
	} else {
		m_compute_queue = m_main_queue;
	}

	if (m_info.dedicated_dma_queue) {
		m_dt.vkGetDeviceQueue(m_handle, m_info.dma_queue_family, 0, &m_dma_queue);
		m_instance.debug().setObjectName(m_handle, m_dma_queue, "common/dma_queue");
	} else {
		m_dma_queue = m_main_queue;
	}
}

void Device::createVma()
{
	VmaVulkanFunctions vma_vk_funcs = {};
	vma_vk_funcs.vkGetInstanceProcAddr = m_instance.dt().vkGetInstanceProcAddr;
	vma_vk_funcs.vkGetDeviceProcAddr = m_instance.dt().vkGetDeviceProcAddr;

	VmaAllocatorCreateInfo vma_create_info {
		// Both maintenance4 (Vulkan 1.3) and maintenance5 are guaranteed by `isSupported()`
		.flags = VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE4_BIT | VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE5_BIT,
		.physicalDevice = m_phys_device.handle(),
		.device = m_handle,
		.preferredLargeHeapBlockSize = 0,
		.pAllocationCallbacks = nullptr,
		.pDeviceMemoryCallbacks = nullptr,
		.pHeapSizeLimit = nullptr,
		.pVulkanFunctions = &vma_vk_funcs,
		.instance = m_instance.handle(),
		.vulkanApiVersion = Capabilities::MIN_VULKAN_VERSION,
		.pTypeExternalMemoryHandleTypes = nullptr,
	};

	if (m_phys_device.extInfo().have_memory_budget) {
		vma_create_info.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
	}

	VkResult res = vmaCreateAllocator(&vma_create_info, &m_vma);
	if (res != VK_SUCCESS) {
		throw VulkanException(res, "vmaCreateAllocator");
	}
}

void Device::createTimelineSemaphore()
{
	VkSemaphoreTypeCreateInfo semaphore_type_info {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		.pNext = nullptr,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		.initialValue = 0,
	};

	VkSemaphoreCreateInfo semaphore_info {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = &semaphore_type_info,
		.flags = 0,
	};

	VkResult res = m_dt.vkCreateSemaphore(m_handle, &semaphore_info, nullptr, &m_timeline_semaphore);
	if (res != VK_SUCCESS) {
		throw VulkanException(res, "vkCreateSemaphore");
	}

	m_instance.debug().setObjectName(m_handle, m_timeline_semaphore, "common/timeline_semaphore");
}

void Device::processDestroyQueue()
{
	// Before erasing elements, check if we have too much queue capacity.
	// This might reduce (very tiny) memory waste after a large "spike"
	// of destroy requests (e.g. unloading something huge).
	// `size + 1` won't needlessly shrink each time the queue is empty.
	if (m_destroy_queue.capacity() > (m_destroy_queue.size() + 1) * 4) {
		m_destroy_queue.shrink_to_fit();
	}

	// Elements are added to the back of `m_destroy_queue`,
	// their timeline value is in non-decreasing order
	auto iter = m_destroy_queue.begin();

	while (iter != m_destroy_queue.end()) {
		if (iter->second > m_last_completed_timeline) {
			// Everything starting from this is not yet safe to destroy
			break;
		}

		std::visit([&](auto &&arg) { destroy(arg); }, iter->first);

		++iter;
	}

	// Preserve the order of remaining elements.
	// Moving around is OK, they are just small handles.
	m_destroy_queue.erase(m_destroy_queue.begin(), iter);
}

void Device::enqueueJunkItem(JunkItem item)
{
	m_destroy_queue.emplace_back(std::move(item), m_last_submitted_timeline);
}

void Device::destroy(std::pair<VkBuffer, VmaAllocation> &obj) noexcept
{
	vmaDestroyBuffer(m_vma, obj.first, obj.second);
}

void Device::destroy(std::pair<VkImage, VmaAllocation> &obj) noexcept
{
	vmaDestroyImage(m_vma, obj.first, obj.second);
}

void Device::destroy(VkImageView obj) noexcept
{
	m_dt.vkDestroyImageView(m_handle, obj, nullptr);
}

void Device::destroy(VkCommandPool pool) noexcept
{
	m_dt.vkDestroyCommandPool(m_handle, pool, nullptr);
}

void Device::destroy(VkDescriptorPool pool) noexcept
{
	m_dt.vkDestroyDescriptorPool(m_handle, pool, nullptr);
}

} // namespace voxen::gfx::vk
