#pragma once

#include <vulkan/vulkan.h>

namespace voxen::gfx::vk
{

class Instance;

// This object wraps no Vulkan functionality (except VkPhysicalDevice handle)
// and only retrieves information about device features/properties, it is
// independent of other objects and can be freely copied and moved around.
// Still, handle is valid onle during its parent `Instance` lifetime.
// NOTE: its sizeof is quite large, you don't want to have this on stack.
class PhysicalDevice {
public:
	// Information retrieved by functions of `vkGetPhysicalDevice*` family.
	// If certain Vulkan version is not supported, its corresponding
	// structures will have undefined contents, so always check it
	// (in `props.properties.apiVersion`) first.
	struct Info {
		// Filled by `vkGetPhysicalDeviceFeatures2`
		VkPhysicalDeviceFeatures2 feats;
		VkPhysicalDeviceVulkan11Features feats11;
		VkPhysicalDeviceVulkan12Features feats12;
		VkPhysicalDeviceVulkan13Features feats13;

		// Filled by `vkGetPhysicalDeviceProperties2`
		VkPhysicalDeviceProperties2 props;
		VkPhysicalDeviceVulkan11Properties props11;
		VkPhysicalDeviceVulkan12Properties props12;
		VkPhysicalDeviceVulkan13Properties props13;

		// Filled by `vkGetPhysicalDeviceMemoryProperties`
		VkPhysicalDeviceMemoryProperties mem_props;
	};

	// Parsed information about queue families available on this device.
	// Any family might be missing (its index field will then be set to
	// `VK_QUEUE_FAMILY_IGNORED`) though it's extremely unlikely to happen
	// with the main queue (unless we're on some compute-only card).
	struct QueueInfo {
		// This family supports all of graphics, compute and transfer operations
		uint32_t main_queue_family = VK_QUEUE_FAMILY_IGNORED;
		// This family supports compute and transfer but not graphics operations
		uint32_t compute_queue_family = VK_QUEUE_FAMILY_IGNORED;
		// This family supports only transfer operations, no compute or graphics
		uint32_t dma_queue_family = VK_QUEUE_FAMILY_IGNORED;

		VkQueueFamilyProperties main_queue_props;
		VkQueueFamilyProperties compute_queue_props;
		VkQueueFamilyProperties dma_queue_props;
	};

	// Information about known extensions support.
	// If a certain extension X is not supported (`have_X = false`)
	// its corresponding `feats_X` and `props_X` structures will
	// have undefined contents, so always check it first.
	struct ExtensionsInfo {
		bool have_maintenance5 = false;
		bool have_memory_budget = false;
		bool have_mesh_shader = false;
		bool have_push_descriptor = false;
		bool have_swapchain = false;

		VkPhysicalDeviceMaintenance5FeaturesKHR feats_maintenance5;
		VkPhysicalDeviceMeshShaderFeaturesEXT feats_mesh_shader;

		VkPhysicalDeviceMaintenance5PropertiesKHR props_maintenance5;
		VkPhysicalDeviceMeshShaderPropertiesEXT props_mesh_shader;
		VkPhysicalDevicePushDescriptorPropertiesKHR props_push_descriptor;
	};

	PhysicalDevice() = default;
	explicit PhysicalDevice(const Instance &instance, VkPhysicalDevice handle);
	PhysicalDevice(PhysicalDevice &&) noexcept = default;
	PhysicalDevice(const PhysicalDevice &other) noexcept = default;
	PhysicalDevice &operator=(PhysicalDevice &&) noexcept = default;
	PhysicalDevice &operator=(const PhysicalDevice &) noexcept = default;
	~PhysicalDevice() noexcept = default;

	VkPhysicalDevice handle() const noexcept { return m_handle; }

	const Info &info() const noexcept { return m_info; }
	const QueueInfo &queueInfo() const noexcept { return m_queue_info; }
	const ExtensionsInfo &extInfo() const noexcept { return m_ext_info; }

private:
	VkPhysicalDevice m_handle = VK_NULL_HANDLE;

	Info m_info = {};
	QueueInfo m_queue_info = {};
	ExtensionsInfo m_ext_info = {};

	std::pair<void *, void *> prepareExtInfoQuery(const Instance &instance);
	void parseQueueInfo(const Instance &instance);
};

} // namespace voxen::gfx::vk
