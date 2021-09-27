#pragma once

#include <voxen/client/vulkan/common.hpp>

#include <cstdint>
#include <vector>

namespace voxen::client::vulkan
{

// Queries and verifies device rendering capabilities. This class is
// a single point of knowledge about what GPU is able to do.
class Capabilities final {
public:
	// This structure defined optional capabilities of the device.
	// These do not affect `selectPhysicalDevice()` decision, but instead are
	// needed for other modules to enable/disable additional rendering paths.
	struct OptionalCaps {
		// Maximal supported image/framebuffer/viewport size.
		// NOTE: this is only what Vulkan implementation reports,
		// windowing system may impose further limits on this.
		VkExtent2D max_frame_size = { 4096, 4096 };
		// Maximal supported degree of anisotropy. Values less than
		// or equal to 1.0f indicate that anisotropy is not supported.
		float max_anisotropy = 0.0f;
		// Maximal number of views the device can render to.
		// Value of 1 indicates there is no multiview support.
		uint32_t max_views = 1;
		// Maximal number of samples per pixel the device can render.
		// Value of 1 indicates multisampling is not supported.
		// NOTE: this limit is the minimal value guaranteed for all supported render target
		// formats. Specific formats can use more samples and need to be queried manually.
		uint32_t max_samples = 1;
		// Maximal number of samples with configurable sample locations.
		// Value of 0 indicates there is no sample locations support.
		uint32_t max_samples_locations = 0;

		// Set to `true` when VK_EXT_depth_range_unrestricted is supported
		bool unrestricted_depth_range_available = false;
		// Set to `true` when advanced depth-stencil resolve
		// modes are supported (MIN/MAX + independent resolve)
		bool advanced_zs_resolve_modes_available = false;
	};

	// Minimal Vulkan API version which must be supported by the device.
	// This is the exact version Voxen is designed to work with:
	// - No fallback paths for earlier API versions are allowed
	// - No optional paths for later API versions are allowed
	// - Using an extension is allowed only if it's not deprecated/obsolete in this version
	constexpr inline static uint32_t MIN_VULKAN_VERSION = VK_API_VERSION_1_2;

	Capabilities() = default;
	Capabilities(Capabilities &&) = delete;
	Capabilities(const Capabilities &) = delete;
	Capabilities &operator = (Capabilities &&) = delete;
	Capabilities &operator = (const Capabilities &) = delete;
	~Capabilities() = default;

	// Fully analyze a given physical device and check whether it is supported.
	// Returns `true` if it satisfies all mandatory Voxen requirements, so logical
	// device can be created from it (and `false` otherwise).
	// NOTE: this class is stateful, and its state is fully replaced by this method.
	// Most other methods return data based on physical device passed to the latest
	// call to this method. Data is undefined if that call had returned `false`.
	// Make sure the latest call to this method was provided the right argument.
	// NOTE: `device` must be externally checked to support `MIN_VULKAN_VERSION`
	// because this method can call functions from any supported Vulkan version.
	bool selectPhysicalDevice(VkPhysicalDevice device);

	// Get list of features to enable when creating logical device for
	// the physical device passed to the latest `selectPhysicalDevice()` call.
	// NOTE: returns undefined data if the latest `selectPhysicalDevice()` call returned `false`.
	const VkPhysicalDeviceFeatures2 &getDeviceFeaturesRequest() const noexcept
		{ return m_dev_creation_request.features10; }
	// Get list of extensions to enable when creating logical device for
	// the physical device passed to the latest `selectPhysicalDevice()` call.
	// NOTE: returns undefined data if the latest `selectPhysicalDevice()` call returned `false`.
	const std::vector<const char *> &getDeviceExtensionsRequest() const noexcept
		{ return m_dev_creation_request.extensions; }

	// Get optional capabilities queried by the latest `selectPhysicalDevice()` call.
	// NOTE: returns undefined data if the latest `selectPhysicalDevice()` call returned `false`.
	const OptionalCaps &optionalCaps() const noexcept { return m_optional_caps; }

	// Get Vulkan 1.0 physical device properties queried by the latest `selectPhysicalDevice()` call
	const VkPhysicalDeviceProperties &props10() const noexcept { return m_phys_dev_caps.props10.properties; }

private:
	OptionalCaps m_optional_caps;

	struct {
		VkPhysicalDeviceFeatures2 features10;
		VkPhysicalDeviceVulkan11Features features11;
		VkPhysicalDeviceVulkan12Features features12;

		VkPhysicalDeviceProperties2 props10;
		VkPhysicalDeviceVulkan11Properties props11;
		VkPhysicalDeviceVulkan12Properties props12;
		VkPhysicalDeviceSampleLocationsPropertiesEXT props_sample_locations;

		std::vector<VkExtensionProperties> extensions;
	} m_phys_dev_caps;

	struct {
		VkPhysicalDeviceFeatures2 features10;
		VkPhysicalDeviceVulkan11Features features11;
		VkPhysicalDeviceVulkan12Features features12;

		std::vector<const char *> extensions;
	} m_dev_creation_request;

	// Returns `true` if selected physical device satisfies all requirements
	// on `VkPhysicalDevice*Features` and `VkPhysicalDevice*Properties`.
	// Also fills part of `m_dev_creation_request.features*`.
	bool checkMandatoryProperties();
	// Fills parts of `m_optional_caps` and `m_dev_creation_request.features*`
	void checkOptionalProperties();
	// Returns `true` if selected physical device has all required extensions.
	// Also fills part of `m_devi_creation_request.extensions`.
	bool checkMandatoryExtensions();
	// Fills parts of `m_optional_caps` and `m_dev_creation_request.extensions`
	void checkOptionalExtensions();
	// Returns `true` if selected physical device satisfies
	// all requirements on per-format capabilities
	bool checkMandatoryFormats(VkPhysicalDevice device);

	// Fill `m_phys_dev_caps` with data for selected physical device
	void fillPhysicalDeviceCaps(VkPhysicalDevice device);
	// Clear all fields/flags of `m_dev_creation_request`
	void prepareDeviceCreationRequest() noexcept;

	bool isExtensionSupported(const char *name) const noexcept;
	// Given a bitmask of supported sample counts, return the largest number of samples
	// such that all counts less than or equal to this are in the mask. For example:
	// `VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_4_BIT` -> 4
	// `VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT` -> 1
	static uint32_t maxSamplesCount(VkSampleCountFlags flags) noexcept;
};

}
