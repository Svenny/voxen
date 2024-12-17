#pragma once

#include <voxen/gfx/vk/vk_include.hpp>

#include <voxen/visibility.hpp>

#include <span>

namespace voxen::gfx::vk
{

struct DebugUtilsDispatchTable {
#define VK_API_ENTRY(x) PFN_##x x = nullptr;
#include "api_debug_utils.in"
#undef VK_API_ENTRY
};

class VOXEN_API DebugUtils {
public:
	class CmdLabelScope {
	public:
		CmdLabelScope(VkCommandBuffer cmd, PFN_vkCmdEndDebugUtilsLabelEXT fn) noexcept : m_cmd(cmd), m_fn(fn) {}
		CmdLabelScope(CmdLabelScope &&) = delete;
		CmdLabelScope(const CmdLabelScope &) = delete;
		CmdLabelScope &operator=(CmdLabelScope &&) = delete;
		CmdLabelScope &operator=(const CmdLabelScope &) = delete;

		~CmdLabelScope() noexcept
		{
			if (m_cmd && m_fn) {
				m_fn(m_cmd);
			}
		}

	private:
		VkCommandBuffer m_cmd;
		PFN_vkCmdEndDebugUtilsLabelEXT m_fn;
	};

	DebugUtils() noexcept = default;
	explicit DebugUtils(VkInstance instance, PFN_vkGetInstanceProcAddr loader);
	DebugUtils(DebugUtils &&other) noexcept;
	DebugUtils(const DebugUtils &) = delete;
	DebugUtils &operator=(DebugUtils &&other) noexcept;
	DebugUtils &operator=(const DebugUtils &) = delete;
	~DebugUtils() noexcept;

	// Whether `VK_EXT_debug_utils` extension is available and loaded.
	// Other methods will do nothing and return no-op stubs if this is `false`.
	bool available() const noexcept { return m_available; }

	// Push debug label region into command buffer.
	// Returned object scopes this label and will automatically pop it upon destruction.
	[[nodiscard]] CmdLabelScope cmdPushLabel(VkCommandBuffer cmd, const char *name, std::span<const float, 4> color);
	[[nodiscard]] CmdLabelScope cmdPushLabel(VkCommandBuffer cmd, const char *name);

	// Set name for an object, will be visible in debugging tools and validation messages
	void setObjectName(VkDevice device, uint64_t handle, VkObjectType type, const char *name) noexcept;

	template<typename T>
	void setObjectName(VkDevice device, T handle, const char *name) noexcept
	{
		setObjectName(device, reinterpret_cast<uint64_t>(handle), objectType<T>(), name);
	}

	template<typename T>
	consteval static VkObjectType objectType()
	{
		if constexpr (std::is_same_v<T, VkBuffer>) {
			return VK_OBJECT_TYPE_BUFFER;
		} else if constexpr (std::is_same_v<T, VkImage>) {
			return VK_OBJECT_TYPE_IMAGE;
		} else if constexpr (std::is_same_v<T, VkImageView>) {
			return VK_OBJECT_TYPE_IMAGE_VIEW;
		} else if constexpr (std::is_same_v<T, VkQueue>) {
			return VK_OBJECT_TYPE_QUEUE;
		} else if constexpr (std::is_same_v<T, VkSemaphore>) {
			return VK_OBJECT_TYPE_SEMAPHORE;
		} else if constexpr (std::is_same_v<T, VkCommandPool>) {
			return VK_OBJECT_TYPE_COMMAND_POOL;
		} else if constexpr (std::is_same_v<T, VkCommandBuffer>) {
			return VK_OBJECT_TYPE_COMMAND_BUFFER;
		} else if constexpr (std::is_same_v<T, VkSampler>) {
			return VK_OBJECT_TYPE_SAMPLER;
		} else {
			static_assert(false, "Unknown handle type");
		}
	}

private:
	bool m_available = false;
	DebugUtilsDispatchTable m_dt;

	VkInstance m_instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT m_messenger = VK_NULL_HANDLE;
};

} // namespace voxen::gfx::vk
