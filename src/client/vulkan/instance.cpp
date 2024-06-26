#include <voxen/client/vulkan/instance.hpp>

#include <voxen/client/gfx_runtime_config.hpp>
#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/capabilities.hpp>
#include <voxen/common/runtime_config.hpp>
#include <voxen/util/error_condition.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>
#include <voxen/version.hpp>

#include <GLFW/glfw3.h>

namespace voxen::client::vulkan
{

Instance::Instance()
{
	Log::debug("Creating Instance");

	if (!checkVulkanSupport()) {
		throw Exception::fromError(VoxenErrc::GfxCapabilityMissing, "unsupported or missing Vulkan driver");
	}

	createInstance();

	auto &backend = Backend::backend();
	if (!backend.loadInstanceLevelApi(m_handle)) {
		// Assuming at least vkDestroyInstance was found...
		destroyInstance();
		throw Exception::fromError(VoxenErrc::GfxCapabilityMissing, "missing required instance-level Vulkan API");
	}

	Log::debug("Instance created successfully");
}

Instance::~Instance() noexcept
{
	Log::debug("Destroying Instance");
	destroyInstance();
	Log::debug("Intance destroyed");
}

bool Instance::checkVulkanSupport() const
{
	if (glfwVulkanSupported() != GLFW_TRUE) {
		Log::error("No supported Vulkan ICD found");
		return false;
	}

	auto &backend = Backend::backend();
	uint32_t version = 0;
	VkResult result = backend.vkEnumerateInstanceVersion(&version);
	if (result != VK_SUCCESS) {
		Log::error("vkEnumerateInstanceVersion failed: {}", VulkanUtils::getVkResultString(result));
		return false;
	}

	uint32_t major = VK_VERSION_MAJOR(version);
	uint32_t minor = VK_VERSION_MINOR(version);
	uint32_t patch = VK_VERSION_PATCH(version);
	Log::info("Vulkan instance version is {}.{}.{}", major, minor, patch);

	if (version < Capabilities::MIN_VULKAN_VERSION) {
		uint32_t req_major = VK_VERSION_MAJOR(Capabilities::MIN_VULKAN_VERSION);
		uint32_t req_minor = VK_VERSION_MINOR(Capabilities::MIN_VULKAN_VERSION);
		Log::error("Vulkan instance version is lower than minimal supported ({}.{})", req_major, req_minor);
		return false;
	}

	return true;
}

static std::vector<const char *> getRequiredInstanceExtensions()
{
	uint32_t glfw_ext_count = 0;
	// GLFW guarantees that on success there will be `VK_KHR_surface` at least
	const char **glfw_ext_list = glfwGetRequiredInstanceExtensions(&glfw_ext_count);

	std::vector<const char *> ext_list;
	ext_list.resize(glfw_ext_count);
	for (uint32_t i = 0; i < glfw_ext_count; i++) {
		ext_list[i] = glfw_ext_list[i];
	}

	// It is an error to request one extension more than once, so we have to check
	// that it's not already in GLFW-provided list before adding it ourselves
	auto addToList = [&](const char *name) {
		for (uint32_t i = 0; i < glfw_ext_count; i++) {
			if (!strcmp(glfw_ext_list[i], name)) {
				return;
			}
		}
		ext_list.emplace_back(name);
	};

	if (RuntimeConfig::instance().gfxConfig().useDebugging()) {
		addToList(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	// TODO: warn about unsupported extensions?
	if (!ext_list.empty()) {
		Log::info("Requesting the following Vulkan instance extensions:");
	}
	for (const char *name : ext_list) {
		Log::info("{}", name);
	}
	return ext_list;
}

static std::vector<const char *> getRequiredLayers()
{
	// Add nothing if validation is not enabled
	if (!RuntimeConfig::instance().gfxConfig().useValidation()) {
		return {};
	}

	auto &backend = Backend::backend();
	uint32_t available_count;
	backend.vkEnumerateInstanceLayerProperties(&available_count, nullptr);
	std::vector<VkLayerProperties> available_props(available_count);
	backend.vkEnumerateInstanceLayerProperties(&available_count, available_props.data());

	if (available_count > 0 && Log::willBeLogged(Log::Level::Debug)) {
		Log::debug("The following Vulkan layers are available:");
		for (const auto &layer : available_props) {
			uint32_t spec_major = VK_VERSION_MAJOR(layer.specVersion);
			uint32_t spec_minor = VK_VERSION_MINOR(layer.specVersion);
			uint32_t spec_patch = VK_VERSION_PATCH(layer.specVersion);
			Log::debug("{} ({}), spec version {}.{}.{}", layer.layerName, layer.description, spec_major, spec_minor,
				spec_patch);
		}
	}

	std::vector<const char *> layer_list;
	// Since layers are used only for debugging, we may just skip requesting
	// unsupported ones. Useful for developing on different machines becuase
	// each machine may have a different set of available layers.
	auto addIfAvailable = [&](const char *name) {
		for (const auto &props : available_props) {
			if (!strcmp(props.layerName, name)) {
				layer_list.emplace_back(name);
				return;
			}
		}
		Log::warn("Attempted to request layer {} which is not available", name);
	};

	addIfAvailable("VK_LAYER_KHRONOS_validation");
	addIfAvailable("VK_LAYER_MESA_overlay");

	if (!layer_list.empty()) {
		Log::info("Requesting the following Vulkan layers:");
	}
	for (const char *name : layer_list) {
		Log::info("{}", name);
	}
	return layer_list;
}

void Instance::createInstance()
{
	// Fill VkApplicationInfo
	auto version = VK_MAKE_VERSION(Version::MAJOR, Version::MINOR, Version::PATCH);
	VkApplicationInfo app_info = {};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "Voxen";
	app_info.applicationVersion = version;
	app_info.pEngineName = "Voxen";
	app_info.engineVersion = version;
	app_info.apiVersion = Capabilities::MIN_VULKAN_VERSION;

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

	auto &backend = Backend::backend();
	VkResult result = backend.vkCreateInstance(&create_info, HostAllocator::callbacks(), &m_handle);
	if (result != VK_SUCCESS) {
		throw VulkanException(result, "vkCreateInstance");
	}
}

void Instance::destroyInstance() noexcept
{
	auto &backend = Backend::backend();
	backend.vkDestroyInstance(m_handle, HostAllocator::callbacks());
	backend.unloadInstanceLevelApi();
}

} // namespace voxen::client::vulkan
