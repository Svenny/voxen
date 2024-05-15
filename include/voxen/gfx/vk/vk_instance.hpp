#pragma once

#include <voxen/gfx/vk/debug_utils.hpp>

namespace voxen::gfx::vk
{

struct InstanceDispatchTable {
#define VK_API_ENTRY(x) PFN_##x x = nullptr;
#include "api_instance.in"
#undef VK_API_ENTRY
};

class Instance {
public:
	// Constructor will automatically attempt to create VkInstance,
	// throwing VoxenErrc::GfxCapabilityMissing if it's not supported.
	// GLFW library must be initialized during the object lifetime.
	Instance();
	Instance(Instance &&) = delete;
	Instance(const Instance &) = delete;
	Instance &operator=(Instance &&) = delete;
	Instance &operator=(const Instance &) = delete;
	~Instance() noexcept;

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
