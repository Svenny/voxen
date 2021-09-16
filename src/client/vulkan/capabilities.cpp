#include <voxen/client/vulkan/capabilities.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/physical_device.hpp>
#include <voxen/util/log.hpp>

#include <bit>
#include <string_view>

namespace voxen::client::vulkan
{

// Policy of software/hardware platform support:
// - The latest "stable" Linux Vulkan drivers are considered target software:
//   - Mesa 3D for Intel (`anv`) and AMD (`radv`)
//   - Proprietary drivers for NVIDIA
// - The following GPUs are considered target hardware:
//   - Intel UHD Graphics 630
//   - AMD Radeon RX Vega 5
//   - AMD Radeon RX 5700
//   - NVIDIA GeForce 1060 GTX
//   - NVIDIA GeForce 1650 GTX

// Policy for classifying extensions and features to 'wanted' or 'mandatory':
// - An extension/feature is marked as mandatory if it is available on all target GPUs
// - Otherwise an extension/feature is marked as wanted and engine parts depending on it must be optional
// - We try to minimize the number of optional code paths, so generally an optional extension
//   is added only if it's either easy to integrate or provides noticeable benefit when enabled

constexpr static const char *MANDATORY_DEVICE_EXTENSIONS[] = {
	// Needed to actually be able to present something.
	// It's dependency `VK_KHR_surface` is guaranteed to be satisfied
	// by GLFW-provided required instance extensions list.
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	// Needed to monitor memory usage, implemented by all sane drivers
	VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
};

bool Capabilities::selectPhysicalDevice(VkPhysicalDevice device)
{
	// Clear optional caps flags
	m_optional_caps = OptionalCaps();
	fillPhysicalDeviceCaps(device);
	prepareDeviceCreationRequest();

	const std::string_view device_name(m_phys_dev_caps.props10.properties.deviceName);
	Log::info("Analyzing capabilities of '{}'", device_name);

	if (!checkMandatoryProperties() || !checkMandatoryExtensions() || !checkMandatoryFormats(device)) {
		Log::info("'{}' doesn't pass minimal system requirements", device_name);
		return false;
	}

	checkOptionalProperties();
	checkOptionalExtensions();

	Log::info("'{}' passes minimal system requirements", device_name);
	return true;
}

bool Capabilities::checkMandatoryProperties()
{
	const auto &caps = m_phys_dev_caps;
	auto &request = m_dev_creation_request;

#define CHECK_MANDATORY_FEATURE(structure, name) \
	if (!caps.structure.name) { \
		Log::info("Mandatory feature '" #name "' is not supported"); \
		return false; \
	} else { \
		request.structure.name = VK_TRUE; \
	}

	// Needed for weighted-blended OIT approximation
	CHECK_MANDATORY_FEATURE(features10.features, independentBlend);
	// Needed for few-drawcall terrain rendering
	CHECK_MANDATORY_FEATURE(features10.features, multiDrawIndirect);
	// Needed to easily pack multiple meshes into a single buffer
	CHECK_MANDATORY_FEATURE(features10.features, drawIndirectFirstInstance);
	// TODO: is it really needed?
	CHECK_MANDATORY_FEATURE(features11, shaderDrawParameters);
	// Setting render targets at the beginning of renderpass is much more convenient
	CHECK_MANDATORY_FEATURE(features12, imagelessFramebuffer);
	// Scalar block layout is used to store some objects without excessive
	// padding (for instance AABBs and Vulkan indirect draw commands)
	CHECK_MANDATORY_FEATURE(features12, scalarBlockLayout);
	// TODO: is it really needed?
	CHECK_MANDATORY_FEATURE(features12, uniformBufferStandardLayout);
	// TODO: is it really needed?
	CHECK_MANDATORY_FEATURE(features12, timelineSemaphore);

#undef CHECK_MANDATORY_FEATURE
	return true;
}

void Capabilities::checkOptionalProperties()
{
	const auto &caps = m_phys_dev_caps;
	auto &request = m_dev_creation_request;

#define CHECK_OPTIONAL_FEATURE(structure, name) \
	if (caps.structure.name) { \
		Log::info("Optional feature '" #name "' is supported"); \
		request.structure.name = VK_TRUE; \
	} else { \
		Log::info("Optional feature '" #name "' is not supported"); \
	}

	// Needed for anisotropic filtering, not so critical if not implemented
	CHECK_OPTIONAL_FEATURE(features10.features, samplerAnisotropy);
	// Needed for cascaded shadow maps
	CHECK_OPTIONAL_FEATURE(features11, multiview);

#undef CHECK_OPTIONAL_FEATURE

	m_optional_caps.max_frame_size.width = caps.props10.properties.limits.maxFramebufferWidth;
	m_optional_caps.max_frame_size.height = caps.props10.properties.limits.maxFramebufferHeight;
	m_optional_caps.max_anisotropy = caps.props10.properties.limits.maxSamplerAnisotropy;
	m_optional_caps.max_views = caps.props11.maxMultiviewViewCount;

	// Take the minimal of all possible values to be conservative.
	// Don't check storage images as they are limited to 1 sample at least on UHD 630.
	m_optional_caps.max_samples = std::min({
		maxSamplesCount(caps.props10.properties.limits.framebufferColorSampleCounts),
		maxSamplesCount(caps.props10.properties.limits.framebufferDepthSampleCounts),
		maxSamplesCount(caps.props10.properties.limits.framebufferStencilSampleCounts),
		maxSamplesCount(caps.props10.properties.limits.sampledImageColorSampleCounts),
		maxSamplesCount(caps.props10.properties.limits.sampledImageIntegerSampleCounts),
		maxSamplesCount(caps.props10.properties.limits.sampledImageDepthSampleCounts),
		maxSamplesCount(caps.props10.properties.limits.sampledImageStencilSampleCounts),
		maxSamplesCount(caps.props12.framebufferIntegerColorSampleCounts)
	});

	// TODO (Svenny): check `max_samples_locations`

	constexpr VkResolveModeFlags resolve_bits = VK_RESOLVE_MODE_MIN_BIT | VK_RESOLVE_MODE_MAX_BIT;
	if (caps.props12.independentResolve == VK_TRUE &&
		(caps.props12.supportedDepthResolveModes & resolve_bits) == resolve_bits &&
		(caps.props12.supportedStencilResolveModes & resolve_bits) == resolve_bits) {
		m_optional_caps.advanced_zs_resolve_modes_available = true;
	}
}

bool Capabilities::checkMandatoryExtensions()
{
	auto &request = m_dev_creation_request;

	request.extensions.assign(std::begin(MANDATORY_DEVICE_EXTENSIONS), std::end(MANDATORY_DEVICE_EXTENSIONS));

	for (const auto &ext : MANDATORY_DEVICE_EXTENSIONS) {
		if (!isExtensionSupported(ext)) {
			Log::info("Mandatory extension '{}' is not supported", ext);
			return false;
		}
	}

	return true;
}

void Capabilities::checkOptionalExtensions()
{
	auto &request = m_dev_creation_request;

#define CHECK_OPTIONAL_EXTENSION(name, action) \
	if (isExtensionSupported(name)) { \
		Log::info("Optional extension '{}' is supported", name); \
		request.extensions.emplace_back(name); \
		action; \
	} else { \
		Log::info("Optional extension '{}' is not supported", name); \
	}

	// Needed for extended depth range, not implemented on Intel UHD 630
	CHECK_OPTIONAL_EXTENSION(VK_EXT_DEPTH_RANGE_UNRESTRICTED_EXTENSION_NAME,
		m_optional_caps.unrestricted_depth_range_available = true);
	// Needed for temporal anti-aliasing
	CHECK_OPTIONAL_EXTENSION(VK_EXT_SAMPLE_LOCATIONS_EXTENSION_NAME,
		m_optional_caps.max_samples_locations = std::min(m_optional_caps.max_samples,
			maxSamplesCount(m_phys_dev_caps.props_sample_locations.sampleLocationSampleCounts)));

#undef CHECK_OPTIONAL_EXTENSION
}

bool Capabilities::checkMandatoryFormats(VkPhysicalDevice device)
{
	struct FormatRequirement {
		VkFormat format = VK_FORMAT_UNDEFINED;
		VkFormatFeatureFlags linear = 0;
		VkFormatFeatureFlags optimal = 0;
		VkFormatFeatureFlags buffer = 0;
	};

	constexpr FormatRequirement requirements[] = {
		// Used as main scene render target
		{ .format = VK_FORMAT_R16G16B16A16_SFLOAT,
		  .optimal = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT },
		// Used as main scene depth render target
		{ .format = VK_FORMAT_D32_SFLOAT,
		  .optimal = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT },
		// Used for weighted-blended OIT approximation "reveal accumulator" buffer
		{ .format = VK_FORMAT_R16_UNORM,
		  .optimal = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT },

		// Output to swapchain will be done in either one of these formats
		{ .format = VK_FORMAT_R8G8B8A8_UNORM, .optimal = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT },
		{ .format = VK_FORMAT_B8G8R8A8_UNORM, .optimal = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT },

		// These are required by almost all mesh formats
		{ .format = VK_FORMAT_R32G32_SFLOAT, .buffer = VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT },
		{ .format = VK_FORMAT_R32G32B32_SFLOAT, .buffer = VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT },
	};

	auto &backend = Backend::backend();

	for (const auto &req : requirements) {
		VkFormatProperties props;
		backend.vkGetPhysicalDeviceFormatProperties(device, req.format, &props);

		if ((props.linearTilingFeatures & req.linear) != req.linear) {
			Log::info("Not all linear tiling features are supported for format '{}'",
			          VulkanUtils::getVkFormatString(req.format));
			return false;
		}
		if ((props.optimalTilingFeatures & req.optimal) != req.optimal) {
			Log::info("Not all optimal tiling features are supported for format '{}'",
			          VulkanUtils::getVkFormatString(req.format));
			return false;
		}
		if ((props.bufferFeatures & req.buffer) != req.buffer) {
			Log::info("Not all buffer features are supported for format '{}'",
			          VulkanUtils::getVkFormatString(req.format));
			return false;
		}
	}

	return true;
}

void Capabilities::fillPhysicalDeviceCaps(VkPhysicalDevice device)
{
	auto &c = m_phys_dev_caps;

	c.features12 = {};
	c.features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

	c.features11 = {};
	c.features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
	c.features11.pNext = &c.features12;

	c.features10 = {};
	c.features10.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	c.features10.pNext = &c.features11;

	c.props_sample_locations = {};
	c.props_sample_locations.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLE_LOCATIONS_PROPERTIES_EXT;

	c.props12 = {};
	c.props12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
	c.props12.pNext = &c.props_sample_locations;

	c.props11 = {};
	c.props11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
	c.props11.pNext = &c.props12;

	c.props10 = {};
	c.props10.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	c.props10.pNext = &c.props11;

	auto &backend = Backend::backend();
	backend.vkGetPhysicalDeviceFeatures2(device, &c.features10);
	backend.vkGetPhysicalDeviceProperties2(device, &c.props10);

	uint32_t num_extensions = 0;
	VkResult res = backend.vkEnumerateDeviceExtensionProperties(device, nullptr, &num_extensions, nullptr);
	if (res != VK_SUCCESS) {
		throw VulkanException(res, "vkEnumerateDeviceExtensionProperties");
	}

	c.extensions.resize(num_extensions);
	res = backend.vkEnumerateDeviceExtensionProperties(device, nullptr, &num_extensions, c.extensions.data());
	if (res != VK_SUCCESS) {
		throw VulkanException(res, "vkEnumerateDeviceExtensionProperties");
	}

	// Sort extensions so binary search can be done on them
	std::sort(c.extensions.begin(), c.extensions.end(), [](const auto &a, const auto &b) {
		return strncmp(a.extensionName, b.extensionName, VK_MAX_EXTENSION_NAME_SIZE) < 0;
	});
}

void Capabilities::prepareDeviceCreationRequest() noexcept
{
	auto &c = m_dev_creation_request;

	c.features12 = {};
	c.features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

	c.features11 = {};
	c.features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
	c.features11.pNext = &c.features12;

	c.features10 = {};
	c.features10.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	c.features10.pNext = &c.features11;

	c.extensions.clear();
	// Heuristic to accomodate a signle memory allocation for optional extensions as well
	c.extensions.reserve(std::size(MANDATORY_DEVICE_EXTENSIONS) * 2);
}

bool Capabilities::isExtensionSupported(const char *name) const noexcept
{
	// Needed to make comparator lambda template be valid for both
	// "extension < name" and "name < extensions" comparisons
	const struct {
		const char *extensionName;
	} wrapper { name };

	return std::binary_search(m_phys_dev_caps.extensions.begin(), m_phys_dev_caps.extensions.end(), wrapper,
	[](const auto &a, const auto &b){
		return strncmp(a.extensionName, b.extensionName, VK_MAX_EXTENSION_NAME_SIZE) < 0;
	});
}

uint32_t Capabilities::maxSamplesCount(VkSampleCountFlags flags) noexcept
{
	// As defined by Vulkan header, `VK_SAMPLE_COUNT_X_BIT` is equal to X,
	// so we can simply count the number of contiguous ones in the bitmask
	return 1u << std::countr_one(flags);
}

}
