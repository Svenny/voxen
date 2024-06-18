#pragma once

#include <voxen/gfx/vk/vk_debug_utils.hpp>

#include <extras/dyn_array.hpp>

namespace voxen::gfx::vk
{

struct InstanceDispatchTable {
#define VK_API_ENTRY(x) PFN_##x x = nullptr;
#include "api_instance.in"
#undef VK_API_ENTRY
};

class PhysicalDevice;

class Instance {
public:
	// Constructor will automatically attempt to create `VkInstance`,
	// throwing `VoxenErrc::GfxCapabilityMissing` if it's not supported.
	// GLFW library must be initialized during the whole object lifetime.
	Instance();
	Instance(Instance &&) = delete;
	Instance(const Instance &) = delete;
	Instance &operator=(Instance &&) = delete;
	Instance &operator=(const Instance &) = delete;
	~Instance() noexcept;

	// Find all `VkPhysicalDevice`s present in the system
	// along with their features and properties. This call
	// take quite a while, there are many queries inside.
	// Returned objects are valid during the object lifetime.
	extras::dyn_array<PhysicalDevice> enumeratePhysicalDevices() const;

	VkInstance handle() const noexcept { return m_handle; }

	// Debug utils functions can always be called regardless
	// of whether debug extension is actually available/enabled
	DebugUtils &debug() noexcept { return m_debug; }

	const InstanceDispatchTable &dt() const noexcept { return m_dt; }

private:
	VkInstance m_handle = VK_NULL_HANDLE;

	DebugUtils m_debug;

	InstanceDispatchTable m_dt;

	void createInstance();
};

} // namespace voxen::gfx::vk
