#include <voxen/gfx/vk/vk_device.hpp>

#include <voxen/gfx/vk/vk_error.hpp>
#include <voxen/gfx/vk/vk_instance.hpp>
#include <voxen/gfx/vk/vk_physical_device.hpp>
#include <voxen/gfx/vk/vk_utils.hpp>
#include <voxen/util/error_condition.hpp>
#include <voxen/util/log.hpp>

#include <extras/defer.hpp>

#include <vma/vk_mem_alloc.h>

#include <cassert>

namespace voxen::gfx::vk
{

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

	createTimelineSemaphores();
	defer_fail {
		for (VkSemaphore semaphore : m_timeline_semaphores) {
			m_dt.vkDestroySemaphore(m_handle, semaphore, nullptr);
		}
	};

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

	// Complete all GPU operations and destroy the remaining queued items
	forceCompletion();

	// Now destroy device subobjects
	for (VkSemaphore semaphore : m_timeline_semaphores) {
		m_dt.vkDestroySemaphore(m_handle, semaphore, nullptr);
	}
	vmaDestroyAllocator(m_vma);

	// And then the device itself
	m_instance.dt().vkDestroyDevice(m_handle, nullptr);

	Log::debug("VkDevice destroyed");
}

uint64_t Device::submitCommands(SubmitInfo info)
{
	assert(info.queue < QueueCount);

	// We need to wrap `VkCommandBuffer` handles in structs
	VkCommandBufferSubmitInfo cmdbuf_info_stack[4];
	std::unique_ptr<VkCommandBufferSubmitInfo[]> cmdbuf_info_heap;

	// Assume a few command buffers (the common case)
	// and don't require heap allocation for them
	VkCommandBufferSubmitInfo *cmdbuf_info = cmdbuf_info_stack;
	if (info.cmds.size() > std::size(cmdbuf_info_stack)) [[unlikely]] {
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

	VkSemaphoreSubmitInfo wait_info[QueueCount + 1] = {};
	// Number of actually used structs
	uint32_t wait_info_count = 0;

	for (auto &[queue, timeline] : info.wait_timelines) {
		assert(queue < QueueCount);

		VkSemaphore semaphore = m_timeline_semaphores[queue];

		auto end = wait_info + wait_info_count;
		auto iter = std::ranges::find(wait_info, end, semaphore, &VkSemaphoreSubmitInfo::semaphore);
		if (iter != end) {
			// It's enough to only wait for the largest value on a single queue
			iter->value = std::max(iter->value, timeline);
			continue;
		}

		// We can append up to `QueueCount` different items
		assert(wait_info_count < std::size(wait_info));

		wait_info[wait_info_count] = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.pNext = nullptr,
			.semaphore = semaphore,
			.value = timeline,
			.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.deviceIndex = 0,
		};
		wait_info_count++;
	}

	if (info.wait_binary_semaphore != VK_NULL_HANDLE) {
		assert(wait_info_count < std::size(wait_info));

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

	// Don't advance the timeline until after the submit.
	// Per Vulkan spec, if `vkQueueSubmit2` fails it must make sure any resource state
	// including synchronization primitives is unaffected, otherwise VK_ERROR_DEVICE_LOST.
	// If we advance it here and fail the submission, we will have an invalid, never
	// submitted, timeline recorded, and someone might accidentally wait on it later.
	uint64_t completion_timeline = m_last_submitted_timelines[info.queue] + 1;

	signal_info[0] = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.pNext = nullptr,
		.semaphore = m_timeline_semaphores[info.queue],
		.value = completion_timeline,
		.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.deviceIndex = 0,
	};

	if (info.signal_binary_semaphore != VK_NULL_HANDLE) {
		signal_info[1] = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.pNext = nullptr,
			.semaphore = info.signal_binary_semaphore,
			.value = 0,
			.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.deviceIndex = 0,
		};
	}

	VkSubmitInfo2 submit {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.pNext = nullptr,
		.flags = 0,
		.waitSemaphoreInfoCount = wait_info_count,
		.pWaitSemaphoreInfos = wait_info,
		.commandBufferInfoCount = static_cast<uint32_t>(info.cmds.size()),
		.pCommandBufferInfos = cmdbuf_info,
		.signalSemaphoreInfoCount = info.signal_binary_semaphore != VK_NULL_HANDLE ? 2u : 1u,
		.pSignalSemaphoreInfos = signal_info,
	};

	VkResult res = m_dt.vkQueueSubmit2(queue(info.queue), 1, &submit, info.signal_fence);
	if (res != VK_SUCCESS) [[unlikely]] {
		throw VulkanException(res, "vkQueueSubmit2");
	}

	// Successfully submitted, advance the timeline
	++m_last_submitted_timelines[info.queue];
	return completion_timeline;
}

void Device::waitForTimeline(Queue queue, uint64_t value)
{
	assert(queue < QueueCount);

	if (value <= m_last_completed_timelines[queue]) {
		// Already complete
		return;
	}

	// First try to check it without waiting
	uint64_t observed_value = 0;
	VkResult res = m_dt.vkGetSemaphoreCounterValue(m_handle, m_timeline_semaphores[queue], &observed_value);
	if (res != VK_SUCCESS) [[unlikely]] {
		throw VulkanException(res, "vkGetSemaphoreCounterValue");
	}

	// Update without `max()`, it can only increase
	m_last_completed_timelines[queue] = observed_value;

	// Is it signaled now?
	// In CPU-bound scenarios we will probably always exit here.
	if (value <= m_last_completed_timelines[queue]) {
		return;
	}

	// Still not signaled, we have to block (GPU-bound or non-pipelined workload)
	VkSemaphoreWaitInfo wait_info {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
		.pNext = nullptr,
		.flags = 0,
		.semaphoreCount = 1,
		.pSemaphores = &m_timeline_semaphores[queue],
		.pValues = &value,
	};

	res = m_dt.vkWaitSemaphores(m_handle, &wait_info, UINT64_MAX);
	if (res != VK_SUCCESS) [[unlikely]] {
		throw VulkanException(res, "vkWaitSemaphores");
	}

	m_last_completed_timelines[queue] = value;
}

void Device::waitForTimelines(std::span<const uint64_t, QueueCount> values)
{
	// We could try checking without waiting first but not sure if that's really needed.
	// Most likely this will be called just once per frame (from `FrameTickSource`).
	VkSemaphoreWaitInfo wait_info {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
		.pNext = nullptr,
		.flags = 0,
		.semaphoreCount = QueueCount,
		.pSemaphores = m_timeline_semaphores,
		.pValues = values.data(),
	};

	VkResult res = m_dt.vkWaitSemaphores(m_handle, &wait_info, UINT64_MAX);
	if (res != VK_SUCCESS) [[unlikely]] {
		throw VulkanException(res, "vkWaitSemaphores");
	}

	// Update completed timeline values.
	// Take maximum as requested values are not necessarily the latest completed ones.
	for (uint32_t i = 0; i < QueueCount; i++) {
		m_last_completed_timelines[i] = std::max(m_last_completed_timelines[i], values[i]);
	}
}

uint64_t Device::getCompletedTimeline(Queue queue)
{
	assert(queue < QueueCount);

	uint64_t value = 0;
	VkResult res = m_dt.vkGetSemaphoreCounterValue(m_handle, m_timeline_semaphores[queue], &value);
	if (res != VK_SUCCESS) [[unlikely]] {
		throw VulkanException(res, "vkGetSemaphoreCounterValue");
	}

	// Update without `max()`, it can only increase
	m_last_completed_timelines[queue] = value;
	return value;
}

void Device::forceCompletion() noexcept
{
	VkResult res = m_dt.vkDeviceWaitIdle(m_handle);
	if (res != VK_SUCCESS) {
		// Most likely VK_ERROR_DEVICE_LOST... whatever, we're about to destroy things
		Log::warn("vkDeviceWaitIdle failed - {}", VulkanUtils::getVkResultString(res));
	}

	// Everything is surely completed now
	for (uint32_t i = 0; i < QueueCount; i++) {
		m_last_completed_timelines[i] = m_last_submitted_timelines[i];
	}

	// Pass bogus value to force destruction of everything
	processDestroyQueue(FrameTickId(INT64_MAX));
}

void Device::onFrameTickBegin(FrameTickId completed_tick, FrameTickId new_tick)
{
	processDestroyQueue(completed_tick);
	m_current_tick_id = new_tick;
}

void Device::onFrameTickEnd(FrameTickId /*current_tick*/)
{
	// Nothing
}

void Device::setObjectName(uint64_t handle, VkObjectType type, const char *name) noexcept
{
	m_instance.debug().setObjectName(m_handle, handle, type, name);
}

DebugUtils &Device::debug() noexcept
{
	return m_instance.debug();
}

// TODO: replace manual list of checks with Vulkan profiles
bool Device::isSupported(PhysicalDevice &pd)
{
	auto &props = pd.info().props.properties;

	Log::debug("Checking GPU '{}' for minimal requirements", props.deviceName);

	// Vulkan version

	uint32_t apiVersion = props.apiVersion;
	uint32_t minVersion = Instance::MIN_VULKAN_VERSION;

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
	if (!ext_info.have_maximal_reconvergence) {
		missing.emplace_back(VK_KHR_SHADER_MAXIMAL_RECONVERGENCE_EXTENSION_NAME);
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
	if (!info.feats.features.shaderInt64) {
		missing.emplace_back("shaderInt64");
	}
	if (!info.feats.features.shaderInt16) {
		missing.emplace_back("shaderInt16");
	}
	if (!info.feats11.storageBuffer16BitAccess) {
		missing.emplace_back("storageBuffer16BitAccess");
	}
	if (!info.feats11.shaderDrawParameters) {
		missing.emplace_back("shaderDrawParameters");
	}
	if (!info.feats12.drawIndirectCount) {
		missing.emplace_back("drawIndirectCount");
	}
	if (!info.feats12.storageBuffer8BitAccess) {
		missing.emplace_back("storageBuffer8BitAccess");
	}
	if (!info.feats12.shaderInt8) {
		missing.emplace_back("shaderInt8");
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
	features12.drawIndirectCount = VK_TRUE;
	features12.storageBuffer8BitAccess = VK_TRUE;
	features12.shaderInt8 = VK_TRUE;
	features12.descriptorIndexing = VK_TRUE;
	features12.scalarBlockLayout = VK_TRUE;
	features12.uniformBufferStandardLayout = VK_TRUE;
	features12.hostQueryReset = VK_TRUE;
	features12.timelineSemaphore = VK_TRUE;
	features12.bufferDeviceAddress = VK_TRUE;
	features12.shaderOutputLayer = VK_TRUE;

	VkPhysicalDeviceVulkan11Features features11 = {};
	features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
	features11.pNext = &features12;
	features11.storageBuffer16BitAccess = VK_TRUE;
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
	features.features.shaderInt64 = VK_TRUE;
	features.features.shaderInt16 = VK_TRUE;

	// Not that these device-local queue priorities matter much...
	// Don't move to inner scope, this is referenced by `vkCreateDevice`.
	constexpr float QUEUE_PRIORITY = 0.5f;

	// Fill VkDeviceQueueCreateInfo's
	std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
	{
		VkDeviceQueueCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		info.queueCount = 1;
		info.pQueuePriorities = &QUEUE_PRIORITY;

		auto &queue_info = m_phys_device.queueInfo();
		// Guaranteed to exist by `isSupported()`
		m_info.main_queue_family = queue_info.main_queue_family;

		m_info.unique_queue_family_count = 1;
		m_info.unique_queue_families[0] = queue_info.main_queue_family;

		info.queueFamilyIndex = queue_info.main_queue_family;
		queue_create_infos.emplace_back(info);

		if (queue_info.dma_queue_family != VK_QUEUE_FAMILY_IGNORED) {
			m_info.dedicated_dma_queue = 1;
			m_info.dma_queue_family = queue_info.dma_queue_family;

			m_info.unique_queue_families[m_info.unique_queue_family_count++] = queue_info.dma_queue_family;

			info.queueFamilyIndex = queue_info.dma_queue_family;
			queue_create_infos.emplace_back(info);
		} else {
			m_info.dma_queue_family = m_info.main_queue_family;
		}

		if (queue_info.compute_queue_family != VK_QUEUE_FAMILY_IGNORED) {
			m_info.dedicated_compute_queue = 1;
			m_info.compute_queue_family = queue_info.compute_queue_family;

			m_info.unique_queue_families[m_info.unique_queue_family_count++] = queue_info.compute_queue_family;

			info.queueFamilyIndex = queue_info.compute_queue_family;
			queue_create_infos.emplace_back(info);
		} else {
			m_info.compute_queue_family = m_info.main_queue_family;
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
		ext_list.emplace_back(VK_KHR_SHADER_MAXIMAL_RECONVERGENCE_EXTENSION_NAME);

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
	static_assert(QueueMain == 0);

	m_dt.vkGetDeviceQueue(m_handle, m_info.main_queue_family, 0, &m_queues[QueueMain]);
	setObjectName(m_queues[QueueMain], "device/queue_main");

	for (uint32_t i = 1; i < QueueCount; i++) {
		m_queues[i] = m_queues[QueueMain];
	}

	if (m_info.dedicated_dma_queue) {
		m_dt.vkGetDeviceQueue(m_handle, m_info.dma_queue_family, 0, &m_queues[QueueDma]);
		setObjectName(m_queues[QueueDma], "device/queue_dma");
	}

	if (m_info.dedicated_compute_queue) {
		m_dt.vkGetDeviceQueue(m_handle, m_info.compute_queue_family, 0, &m_queues[QueueCompute]);
		setObjectName(m_queues[QueueCompute], "device/queue_compute");
	}
}

void Device::createVma()
{
	VmaVulkanFunctions vma_vk_funcs = {};
	vma_vk_funcs.vkGetInstanceProcAddr = m_instance.dt().vkGetInstanceProcAddr;
	vma_vk_funcs.vkGetDeviceProcAddr = m_instance.dt().vkGetDeviceProcAddr;

	VmaAllocatorCreateInfo vma_create_info {
		// Both maintenance4 (Vulkan 1.3) and maintenance5 are guaranteed by `isSupported()`
		.flags = VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE4_BIT | VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE5_BIT
			| VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
		.physicalDevice = m_phys_device.handle(),
		.device = m_handle,
		.preferredLargeHeapBlockSize = 0,
		.pAllocationCallbacks = nullptr,
		.pDeviceMemoryCallbacks = nullptr,
		.pHeapSizeLimit = nullptr,
		.pVulkanFunctions = &vma_vk_funcs,
		.instance = m_instance.handle(),
		.vulkanApiVersion = Instance::MIN_VULKAN_VERSION,
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

void Device::createTimelineSemaphores()
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

	const char *names[] = { "main", "dma", "compute" };
	static_assert(std::size(names) == QueueCount);

	for (uint32_t i = 0; i < QueueCount; i++) {
		VkResult res = m_dt.vkCreateSemaphore(m_handle, &semaphore_info, nullptr, &m_timeline_semaphores[i]);
		if (res != VK_SUCCESS) {
			throw VulkanException(res, "vkCreateSemaphore");
		}

		char buf[32];
		snprintf(buf, std::size(buf), "device/timeline_%s", names[i]);
		setObjectName(m_timeline_semaphores[i], buf);
	}
}

void Device::processDestroyQueue(FrameTickId completed_tick) noexcept
{
	// Before erasing elements, check if we have too much queue capacity.
	// This might reduce (very tiny) memory waste after a large "spike"
	// of destroy requests (e.g. unloading something huge).
	// `size + 1` won't needlessly shrink each time the queue is empty.
	if (m_destroy_queue.capacity() > (m_destroy_queue.size() + 1) * 4) [[unlikely]] {
		m_destroy_queue.shrink_to_fit();
	}

	// Elements are added to the back of `m_destroy_queue`,
	// their tick ID value is in non-decreasing order.
	// After the loop this iterator will point to the first "unsafe" item.
	auto iter = m_destroy_queue.begin();

	while (iter != m_destroy_queue.end()) {
		if (iter->second > completed_tick) {
			// Frame ticks can only increase, and items are appended in chronological
			// order. Once we get recorded tick exceeding the completed one it will
			// stay true, meaning all remaining items are not yet safe to destroy.
			break;
		}

		// `destroy()` is overloaded for every supported handle type
		std::visit([&](auto &&arg) { destroy(arg); }, iter->first);

		++iter;
	}

	// Preserve the order of remaining elements.
	// Moving around is OK, they are just small handles.
	m_destroy_queue.erase(m_destroy_queue.begin(), iter);
}

void Device::enqueueJunkItem(JunkItem item)
{
	m_destroy_queue.emplace_back(std::move(item), m_current_tick_id);
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

void Device::destroy(VkSwapchainKHR swapchain) noexcept
{
	m_dt.vkDestroySwapchainKHR(m_handle, swapchain, nullptr);
}

void Device::destroy(VkSampler sampler) noexcept
{
	m_dt.vkDestroySampler(m_handle, sampler, nullptr);
}

} // namespace voxen::gfx::vk
