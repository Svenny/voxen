#include <voxen/gfx/vk/vk_physical_device.hpp>

#include <voxen/client/vulkan/common.hpp>
#include <voxen/gfx/vk/vk_instance.hpp>

namespace voxen::gfx::vk
{

// TODO: not yet moved to voxen/gfx/vk
using client::vulkan::VulkanException;

PhysicalDevice::PhysicalDevice(const Instance &instance, VkPhysicalDevice handle) : m_handle(handle)
{
	auto &dt = instance.dt();

	auto [feats_pnext_last, props_pnext_last] = prepareExtInfoQuery(instance);

	m_info.feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	m_info.feats.pNext = &m_info.feats11;
	m_info.feats11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
	m_info.feats11.pNext = &m_info.feats12;
	m_info.feats12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	m_info.feats12.pNext = &m_info.feats13;
	m_info.feats13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	m_info.feats13.pNext = feats_pnext_last;

	dt.vkGetPhysicalDeviceFeatures2(m_handle, &m_info.feats);

	m_info.props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	m_info.props.pNext = &m_info.props11;
	m_info.props11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
	m_info.props11.pNext = &m_info.props12;
	m_info.props12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
	m_info.props12.pNext = &m_info.props13;
	m_info.props13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES;
	m_info.props13.pNext = props_pnext_last;

	dt.vkGetPhysicalDeviceProperties2(m_handle, &m_info.props);

	dt.vkGetPhysicalDeviceMemoryProperties(m_handle, &m_info.mem_props);

	parseQueueInfo(instance);
}

void PhysicalDevice::parseQueueInfo(const Instance &instance)
{
	auto &dt = instance.dt();

	uint32_t family_count = 0;
	dt.vkGetPhysicalDeviceQueueFamilyProperties(m_handle, &family_count, nullptr);

	extras::dyn_array<VkQueueFamilyProperties> family_props(family_count);
	dt.vkGetPhysicalDeviceQueueFamilyProperties(m_handle, &family_count, family_props.data());

	constexpr VkQueueFlags MAIN_QUEUE_BITS = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
	// DMA queues can also do sparse binding but should not do anything else.
	// E.g. there can be VIDEO_ENCODE/DECODE/OPTICAL_FLOW queues with
	// TRANSFER but no GRAPHICS/COMPUTE bits - they are not DMA queues.
	constexpr VkQueueFlags DMA_ADDITIONAL_BITS = VK_QUEUE_SPARSE_BINDING_BIT;

	for (uint32_t i = 0; i < family_count; i++) {
		const VkQueueFamilyProperties &family = family_props[i];

		if ((family.queueFlags & MAIN_QUEUE_BITS) == MAIN_QUEUE_BITS) {
			// Both graphics and compute => main queue
			m_queue_info.main_queue_family = i;
			m_queue_info.main_queue_props = family;
		} else if (family.queueFlags & VK_QUEUE_COMPUTE_BIT) {
			// Compute but no graphics => compute queue
			m_queue_info.compute_queue_family = i;
			m_queue_info.compute_queue_props = family;
		} else if ((family.queueFlags & ~DMA_ADDITIONAL_BITS) == VK_QUEUE_TRANSFER_BIT) {
			// Transfer but neither graphics nor compute, no special-purpose bits => DMA queue
			m_queue_info.dma_queue_family = i;
			m_queue_info.dma_queue_props = family;
		}
	}
}

std::pair<void *, void *> PhysicalDevice::prepareExtInfoQuery(const Instance &instance)
{
	auto &dt = instance.dt();

	uint32_t num_exts = 0;
	VkResult res = dt.vkEnumerateDeviceExtensionProperties(m_handle, nullptr, &num_exts, nullptr);
	if (res != VK_SUCCESS) {
		throw VulkanException(res, "vkEnumerateDeviceExtensionProperties");
	}

	extras::dyn_array<VkExtensionProperties> ext_props(num_exts);
	res = dt.vkEnumerateDeviceExtensionProperties(m_handle, nullptr, &num_exts, ext_props.data());
	if (res != VK_SUCCESS) {
		throw VulkanException(res, "vkEnumerateDeviceExtensionProperties");
	}

	void *feats_pnext_last = nullptr;
	void *props_pnext_last = nullptr;

	for (const auto &ext : ext_props) {
		if (!strcmp(ext.extensionName, VK_KHR_MAINTENANCE_5_EXTENSION_NAME)) {
			m_ext_info.have_maintenance5 = true;

			m_ext_info.feats_maintenance5.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR;
			m_ext_info.feats_maintenance5.pNext = std::exchange(feats_pnext_last, &m_ext_info.feats_maintenance5);

			m_ext_info.props_maintenance5.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_PROPERTIES_KHR;
			m_ext_info.props_maintenance5.pNext = std::exchange(props_pnext_last, &m_ext_info.props_maintenance5);
		} else if (!strcmp(ext.extensionName, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME)) {
			m_ext_info.have_memory_budget = true;
		} else if (!strcmp(ext.extensionName, VK_EXT_MESH_SHADER_EXTENSION_NAME)) {
			m_ext_info.have_mesh_shader = true;

			m_ext_info.feats_mesh_shader.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
			m_ext_info.feats_mesh_shader.pNext = std::exchange(feats_pnext_last, &m_ext_info.feats_mesh_shader);

			m_ext_info.props_mesh_shader.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT;
			m_ext_info.props_mesh_shader.pNext = std::exchange(props_pnext_last, &m_ext_info.props_mesh_shader);
		} else if (!strcmp(ext.extensionName, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME)) {
			m_ext_info.have_push_descriptor = true;

			m_ext_info.props_push_descriptor.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR;
			m_ext_info.props_push_descriptor.pNext = std::exchange(props_pnext_last, &m_ext_info.props_push_descriptor);
		} else if (!strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
			m_ext_info.have_swapchain = true;
		}
	}

	return { feats_pnext_last, props_pnext_last };
}

} // namespace voxen::gfx::vk
