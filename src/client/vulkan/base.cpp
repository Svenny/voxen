#include <voxen/client/vulkan/base.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>
#include <voxen/config.hpp>

#include <GLFW/glfw3.h>

#include <tuple>

namespace voxen::client
{

VulkanBase::VulkanBase() {
	if (!checkVulkanSupport())
		throw MessageException("unsupported or missing Vulkan driver");
	if (!createInstance())
		throw MessageException("failed to create Vulkan instance");
}

VulkanBase::~VulkanBase() {
	Log::debug("Destroying VkInstance");
	vkDestroyInstance(m_instance, VulkanHostAllocator::callbacks());
}

bool VulkanBase::checkVulkanSupport() const {
	if (glfwVulkanSupported() != GLFW_TRUE) {
		Log::error("No supported Vulkan ICD found");
		return false;
	}

	uint32_t version = 0;
	VkResult result = vkEnumerateInstanceVersion(&version);
	if (result != VK_SUCCESS) {
		Log::error("vkEnumerateInstanceVersion failed: {}", getVkResultString(result));
		return false;
	}

	uint32_t major = VK_VERSION_MAJOR(version);
	uint32_t minor = VK_VERSION_MINOR(version);
	uint32_t patch = VK_VERSION_PATCH(version);
	Log::info("Vulkan instance version is {}.{}.{}", major, minor, patch);
	if (major < kMinVulkanVersionMajor || (major == kMinVulkanVersionMajor && minor < kMinVulkanVersionMinor)) {
		Log::error("Vulkan instance version is lower than minimal supported {}.{}",
		           kMinVulkanVersionMajor, kMinVulkanVersionMinor);
		return false;
	}
	return true;
}

static std::vector<const char *> getRequiredInstanceExtensions() {
	uint32_t glfw_ext_count = 0;
	const char **glfw_ext_list = glfwGetRequiredInstanceExtensions(&glfw_ext_count);

	std::vector<const char *> ext_list;
	ext_list.resize(glfw_ext_count);
	for (uint32_t i = 0; i < glfw_ext_count; i++)
		ext_list[i] = glfw_ext_list[i];

	// It is an error to request one extension more than once, so we have to check
	// that it's not already in GLFW-provided list before adding it ourselves
	auto addToList = [&](const char *name) {
		for (uint32_t i = 0; i < glfw_ext_count; i++)
			if (!strcmp(glfw_ext_list[i], name))
				return;
		ext_list.emplace_back(name);
	};

	if constexpr (BuildConfig::kUseVulkanDebugging) {
		addToList("VK_EXT_debug_utils");
	}

	// TODO: warn about unsupported extensions?
	if (!ext_list.empty())
		Log::info("Requesting the following Vulkan instance extensions:");
	for (const char *name : ext_list)
		Log::info("{}", name);
	return ext_list;
}

static std::vector<const char *> getRequiredLayers() {
	if constexpr (!BuildConfig::kUseVulkanDebugging)
	      return {};

	uint32_t available_count;
	vkEnumerateInstanceLayerProperties(&available_count, nullptr);
	std::vector<VkLayerProperties> available_props(available_count);
	vkEnumerateInstanceLayerProperties(&available_count, available_props.data());

	std::vector<const char *> layer_list;
	// Since layers are used only for debugging, we may just skip requesting
	// unsupported ones. Useful for developing on different machines becuase
	// each machine may have a different set of available layers.
	auto addIfAvailable = [&](const char *name) {
		for (const auto &props: available_props) {
			if (!strcmp(props.layerName, name)) {
				layer_list.emplace_back(name);
				return;
			}
		}
		Log::warn("Attempted to request validation layer {} which is not available", name);
	};

	addIfAvailable("VK_LAYER_KHRONOS_validation");
	addIfAvailable("VK_LAYER_LUNARG_standard_validation");
	addIfAvailable("VK_LAYER_MESA_overlay");

	if (!layer_list.empty())
		Log::info("Requesting the following Vulkan validation layers:");
	for (const char *name : layer_list)
		Log::info("{}", name);
	return layer_list;
}

bool VulkanBase::createInstance() {
	// Fill VkApplicationInfo
	auto version = VK_MAKE_VERSION(BuildConfig::kVersionMajor, BuildConfig::kVersionMinor, BuildConfig::kVersionPatch);
	VkApplicationInfo app_info = {};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "Voxen";
	app_info.applicationVersion = version;
	app_info.pEngineName = "Voxen";
	app_info.engineVersion = version;
	app_info.apiVersion = VK_MAKE_VERSION(kMinVulkanVersionMajor, kMinVulkanVersionMinor, 0);

	// Fill VkInstanceCreateInfo
	auto ext_list = getRequiredInstanceExtensions();
	auto layer_list = getRequiredLayers();
	VkInstanceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;
	create_info.enabledExtensionCount = uint32_t(ext_list.size());
	create_info.ppEnabledExtensionNames = ext_list.data();
	create_info.enabledLayerCount = uint32_t(layer_list.size());
	create_info.ppEnabledLayerNames = layer_list.data();

	VkResult result = vkCreateInstance(&create_info, VulkanHostAllocator::callbacks(), &m_instance);
	if (result != VK_SUCCESS) {
		Log::error("vkCreateInstance failed: {}", getVkResultString(result));
		return false;
	}
	Log::debug("VkInstance constructed successfully");
	return true;
}

}
