#pragma once

#include <voxen/client/vulkan/image.hpp>

#include <voxen/client/graphics_options.hpp>

#include <cstdint>
#include <unordered_map>

namespace voxen::client::vulkan
{

enum class RenderTarget : uint32_t {
	// Does not denote any target
	None = 0,
	// Scene HDR color buffer with multisampling. Will not exist if
	// selected AA method does not use more than one spatial sample.
	SceneHdrColorSamples,
	// Scene HDR color buffer after MSAA resolve
	SceneHdrColorResolved,
	// Scene depth-stencil buffer with multisampling. Will not exist if
	// selected AA method does not use more than one spatial sample.
	SceneDepthStencilSamples,
	// Scene depth-stencil buffer after MSAA resolve
	SceneDepthStencilResolved,
	// Color accumulation buffer for weighted-blended OIT
	OitColor,
	// Reveal accumulation buffer for weighted-blended OIT
	OitReveal,
	// Combined scene color buffer after all passes (HDR, AA, OIT etc.).
	SceneFinal,
	// Swapchain image. Not fully controlled by `PerFrameResourcesStorage`.
	Swapchain,

	EnumMax
};

const char *getTargetName(RenderTarget target) noexcept;

enum class RenderTargetFormatClass : uint32_t {
	// Special invalid value, will trigger graph compile error if met in description
	Invalid = 0,
	// Format of HDR scene color buffer
	SceneHdrColor,
	// Format of LDR/resolved scene color buffer
	SceneFinalColor,
	// Format of scene depth/stencil buffer
	SceneDepthStencil,
	// Format of OIT accumulator buffer
	OitAccum,
	// Format of OIT reveal buffer
	OitReveal,
	// Format of swapchain images
	Swapchain
};

enum class RenderTargetDimensionsClass : uint32_t {
	// Special invalid value, will trigger graph compile error if met in description
	Invalid = 0,
	// The target is a 2D resource which size is equal to that of scene color buffer
	Scene,
	// The target is a 2D resource which size is equal to that of swapchain image
	Window,
};

enum class RenderTargetSamplesClass : uint32_t {
	// Special invalid value, will trigger graph compile error if met in description
	Invalid = 0,
	// The target always has one sample or is not a multisample-capable resource
	One,
	// The target has N samples as set by used AA method's spatial samples
	ByAaMethod
};

class PerFrameResourcesStorage {
public:
	PerFrameResourcesStorage(const GraphicsOptions &opts);
	PerFrameResourcesStorage(PerFrameResourcesStorage &&) = delete;
	PerFrameResourcesStorage(const PerFrameResourcesStorage &) = delete;
	PerFrameResourcesStorage &operator = (PerFrameResourcesStorage &&) = delete;
	PerFrameResourcesStorage &operator = (const PerFrameResourcesStorage &) = delete;
	~PerFrameResourcesStorage() noexcept;

	RenderTargetFormatClass getTargetFormat(RenderTarget target) const;
	RenderTargetDimensionsClass getTargetDimensions(RenderTarget target) const;
	RenderTargetSamplesClass getTargetSamples(RenderTarget target) const;

	void requestTargetTemporalCopy(RenderTarget target, uint32_t offset);
	void requestTargetMipLevel(RenderTarget target, uint32_t level);
	void requestTargetArrayLayer(RenderTarget target, uint32_t layer);
	void requestTargetUsage(RenderTarget target, VkImageUsageFlags usage);

	void createResources(const GraphicsOptions &opts);
	void advanceTemporalCopies(VkImage swapchain_image);

private:

	struct TargetInfo {
		std::vector<VkImage> temporal_copies{};
		uint32_t cur_temporal_copy = 0;

		VkImageCreateFlags create_flags = 0;
		VkImageType create_type = VK_IMAGE_TYPE_2D;
		VkFormat create_format = VK_FORMAT_UNDEFINED;
		VkExtent3D create_extent = { 0, 0, 0 };
		VkSampleCountFlagBits create_samples = VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;
		VkImageTiling create_tiling = VK_IMAGE_TILING_OPTIMAL;
		VkImageUsageFlags create_usage = 0;

		uint32_t num_mip_levels = 0;
		uint32_t num_array_layers = 0;

		const RenderTargetFormatClass format = RenderTargetFormatClass::Invalid;
		const RenderTargetDimensionsClass dimensions = RenderTargetDimensionsClass::Invalid;
		const RenderTargetSamplesClass samples = RenderTargetSamplesClass::Invalid;
	};

	std::unordered_map<RenderTarget, TargetInfo> m_info;
	bool m_created_resources = false;
};

}
