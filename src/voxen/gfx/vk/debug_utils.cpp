#include <voxen/gfx/vk/debug_utils.hpp>

// TODO: include not moved to `gfx/vk`
#include <voxen/client/vulkan/common.hpp>
#include <voxen/util/log.hpp>

namespace voxen::gfx::vk
{

namespace
{

VkBool32 debugMessageCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
	void * /*pUserData*/) noexcept
{
	Log::Level level = Log::Level::Info;

	if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		level = Log::Level::Error;
	} else if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		level = Log::Level::Warn;
	}

	bool spec = !!(messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT);
	bool perf = !!(messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT);

	Log::log(level, extras::source_location::current(), "Vulkan debug message ({}):\n{}",
		spec && perf ? "spec+perf" : (spec ? "spec" : "perf"), pCallbackData->pMessage);

	return VK_FALSE;
}

} // namespace

DebugUtils::DebugUtils(VkInstance instance, PFN_vkGetInstanceProcAddr loader) : m_instance(instance)
{
#define VK_API_ENTRY(x) \
	m_dt.x = reinterpret_cast<PFN_##x>(loader(instance, #x)); \
	if (!m_dt.x) { \
		Log::error("Can't load '{}', considering debug utils unavailable", #x); \
		return; \
	}
#include <voxen/gfx/vk/api_debug_utils.in>
#undef VK_API_ENTRY

	VkDebugUtilsMessengerCreateInfoEXT msg_create_info {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.pNext = nullptr,
		.flags = 0,
		.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
			| VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
		.pfnUserCallback = &debugMessageCallback,
		.pUserData = nullptr,
	};

	VkResult res = m_dt.vkCreateDebugUtilsMessengerEXT(instance, &msg_create_info, nullptr, &m_messenger);
	if (res != VK_SUCCESS) {
		// Shouldn't happen unless OOM
		Log::warn("vkCreateDebugUtilsMessengerEXT returned {}!", client::vulkan::VulkanUtils::getVkResultString(res));
	}

	m_available = true;
}

DebugUtils::DebugUtils(DebugUtils &&other) noexcept
{
	*this = std::move(other);
}

DebugUtils &DebugUtils::operator=(DebugUtils &&other) noexcept
{
	std::swap(m_available, other.m_available);
	std::swap(m_dt, other.m_dt);
	std::swap(m_instance, other.m_instance);
	std::swap(m_messenger, other.m_messenger);
	return *this;
}

DebugUtils::~DebugUtils() noexcept
{
	if (m_messenger) {
		m_dt.vkDestroyDebugUtilsMessengerEXT(m_instance, m_messenger, nullptr);
	}
}

DebugUtils::CmdLabelScope DebugUtils::cmdPushLabel(VkCommandBuffer cmd, const char *name,
	std::span<const float, 4> color)
{
	if (!m_available) {
		return CmdLabelScope(VK_NULL_HANDLE, nullptr);
	}

	VkDebugUtilsLabelEXT label {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
		.pNext = nullptr,
		.pLabelName = name,
		.color = { color[0], color[1], color[2], color[3] },
	};

	m_dt.vkCmdBeginDebugUtilsLabelEXT(cmd, &label);

	return CmdLabelScope(cmd, m_dt.vkCmdEndDebugUtilsLabelEXT);
}

DebugUtils::CmdLabelScope DebugUtils::cmdPushLabel(VkCommandBuffer cmd, const char *name)
{
	constexpr float color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	return cmdPushLabel(cmd, name, color);
}

void DebugUtils::setObjectName(VkDevice device, uint64_t handle, VkObjectType type, const char *name)
{
	if (!m_available) {
		return;
	}

	VkDebugUtilsObjectNameInfoEXT name_info {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.pNext = nullptr,
		.objectType = type,
		.objectHandle = handle,
		.pObjectName = name,
	};

	VkResult res = m_dt.vkSetDebugUtilsObjectNameEXT(device, &name_info);
	if (res != VK_SUCCESS) {
		// Shouldn't happen unless OOM
		Log::warn("vkSetDebugUtilsObjectNameEXT({}) returned {}!", name,
			client::vulkan::VulkanUtils::getVkResultString(res));
	}
}

} // namespace voxen::gfx::vk
