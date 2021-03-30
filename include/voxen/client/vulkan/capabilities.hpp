#pragma once

#include <voxen/client/vulkan/common.hpp>

#include <cstdint>
#include <vector>

namespace voxen::client::vulkan
{

// Queries and lists subsystem rendering capabilities
class Capabilities final {
public:
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

		// Set to `true` when VK_EXT_depth_range_unrestricted is supported
		bool unrestricted_depth_range_available = false;
		// Set to `true` when VK_AMD_rasterization_order is supported
		bool relaxed_raster_order_available = false;
	};

	constexpr static uint32_t MIN_VULKAN_VERSION = VK_API_VERSION_1_2;

	Capabilities() = default;
	Capabilities(Capabilities &&) = delete;
	Capabilities(const Capabilities &) = delete;
	Capabilities &operator = (Capabilities &&) = delete;
	Capabilities &operator = (const Capabilities &) = delete;
	~Capabilities() = default;

	// NOTE: `device` must be externally checked to support (MIN_VULKAN_MAJOR, MIN_VULKAN_MINOR)
	// API version because this function does calls requiring recent Vulkan versions.
	bool selectPhysicalDevice(VkPhysicalDevice device);

	const VkPhysicalDeviceFeatures2 &getDeviceFeaturesRequest() const noexcept
		{ return m_dev_creation_request.features10; }
	const std::vector<const char *> &getDeviceExtensionsRequest() const noexcept
		{ return m_dev_creation_request.extensions; }

	const OptionalCaps &optionalCaps() const noexcept { return m_optional_caps; }

private:
	OptionalCaps m_optional_caps;

	struct {
		VkPhysicalDeviceFeatures2 features10;
		VkPhysicalDeviceVulkan11Features features11;
		VkPhysicalDeviceVulkan12Features features12;

		VkPhysicalDeviceProperties2 props10;
		VkPhysicalDeviceVulkan11Properties props11;
		VkPhysicalDeviceVulkan12Properties props12;

		std::vector<VkExtensionProperties> extensions;
	} m_phys_dev_caps;

	struct {
		VkPhysicalDeviceFeatures2 features10;
		VkPhysicalDeviceVulkan11Features features11;
		VkPhysicalDeviceVulkan12Features features12;

		std::vector<const char *> extensions;
	} m_dev_creation_request;

	bool checkMandatoryProperties();
	void checkOptionalProperties();
	bool checkMandatoryExtensions();
	void checkOptionalExtensions();
	bool checkMandatoryFormats(VkPhysicalDevice device);

	void fillPhysicalDeviceCaps(VkPhysicalDevice device);
	void prepareDeviceCreationRequest() noexcept;

	bool isExtensionSupported(const char *name) const noexcept;
};

}
