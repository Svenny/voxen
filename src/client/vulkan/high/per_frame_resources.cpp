#include <voxen/client/vulkan/high/per_frame_resources.hpp>

#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/device.hpp>
#include <voxen/client/vulkan/memory.hpp>

#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

namespace voxen::client::vulkan
{

const char *getTargetName(RenderTarget target) noexcept
{
	switch (target) {
	case RenderTarget::None: return "None";
	case RenderTarget::SceneHdrColorSamples: return "SceneHdrColorSamples";
	case RenderTarget::SceneHdrColorResolved: return "SceneHdrColorResolved";
	case RenderTarget::SceneDepthStencilSamples: return "SceneDepthStencilSamples";
	case RenderTarget::SceneDepthStencilResolved: return "SceneDepthStencilResolved";
	case RenderTarget::OitColor: return "OitColor";
	case RenderTarget::OitReveal: return "OitReveal";
	case RenderTarget::SceneFinal: return "SceneFinal";
	case RenderTarget::Swapchain: return "Swapchain";
	default: return "[UNKNOWN]";
	}
}

PerFrameResourcesStorage::PerFrameResourcesStorage(const GraphicsOptions &opts)
{
	bool has_samples;

	switch (opts.aaMethod()) {
	case GraphicsOptions::AaMethod::Msaa2x: [[fallthrough]];
	case GraphicsOptions::AaMethod::Msaa4x: [[fallthrough]];
	case GraphicsOptions::AaMethod::Msaa8x:
		has_samples = true;
		break;
	case GraphicsOptions::AaMethod::None: [[fallthrough]];
	case GraphicsOptions::AaMethod::Taa1S2T: [[fallthrough]];
	case GraphicsOptions::AaMethod::Taa1S4T: [[fallthrough]];
	case GraphicsOptions::AaMethod::Taa1S8T:
		has_samples = false;
		break;
	default:
		Log::error("Unknown AA method is selected. This is a bug in Voxen.");
		throw MessageException("unknown AA method");
	}

	if (has_samples) {
		m_info.emplace(RenderTarget::SceneHdrColorSamples, TargetInfo {
			.format = RenderTargetFormatClass::SceneHdrColor,
			.dimensions = RenderTargetDimensionsClass::Scene,
			.samples = RenderTargetSamplesClass::ByAaMethod
		});
		m_info.emplace(RenderTarget::SceneDepthStencilSamples, TargetInfo {
			.format = RenderTargetFormatClass::SceneDepthStencil,
			.dimensions = RenderTargetDimensionsClass::Scene,
			.samples = RenderTargetSamplesClass::ByAaMethod
		});
	}

	m_info.emplace(RenderTarget::SceneHdrColorResolved, TargetInfo {
		.format = RenderTargetFormatClass::SceneHdrColor,
		.dimensions = RenderTargetDimensionsClass::Scene,
		.samples = RenderTargetSamplesClass::One
	});
	m_info.emplace(RenderTarget::SceneDepthStencilResolved, TargetInfo {
		.format = RenderTargetFormatClass::SceneDepthStencil,
		.dimensions = RenderTargetDimensionsClass::Scene,
		.samples = RenderTargetSamplesClass::One
	});
	m_info.emplace(RenderTarget::OitColor, TargetInfo {
		.format = RenderTargetFormatClass::OitAccum,
		.dimensions = RenderTargetDimensionsClass::Scene,
		.samples = RenderTargetSamplesClass::One
	});
	m_info.emplace(RenderTarget::OitReveal, TargetInfo {
		.format = RenderTargetFormatClass::OitReveal,
		.dimensions = RenderTargetDimensionsClass::Scene,
		.samples = RenderTargetSamplesClass::One
	});
	m_info.emplace(RenderTarget::SceneFinal, TargetInfo {
		.format = RenderTargetFormatClass::SceneFinalColor,
		.dimensions = RenderTargetDimensionsClass::Scene,
		.samples = RenderTargetSamplesClass::One
	});
	m_info.emplace(RenderTarget::Swapchain, TargetInfo {
		.format = RenderTargetFormatClass::Swapchain,
		.dimensions = RenderTargetDimensionsClass::Window,
		.samples = RenderTargetSamplesClass::One
	});
}

PerFrameResourcesStorage::~PerFrameResourcesStorage() noexcept
{
	auto &backend = Backend::backend();
	VkDevice device = backend.device();

	for (auto &[target, info] : m_info) {
		if (target == RenderTarget::Swapchain) {
			continue;
		}

		for (VkImage image : info.temporal_copies) {
			backend.vkDestroyImage(device, image, VulkanHostAllocator::callbacks());
		}
	}
}

void PerFrameResourcesStorage::requestTargetTemporalCopy(RenderTarget target, uint32_t offset)
{
	if (target == RenderTarget::Swapchain && offset != 0) {
		Log::error("Requested temporal copy N-{} for swapchain render target (only N is allowed)", offset);
		throw MessageException("invalid usage of swapchain render target");
	}

	auto iter = m_info.find(target);
	if (iter == m_info.end()) {
		Log::error("Requested target '{}' does not exist in this static configuration", getTargetName(target));
		throw MessageException("access to non-existing render target");
	}

	size_t new_size = std::max(iter->second.temporal_copies.size(), size_t(offset + 1));
	iter->second.temporal_copies.resize(new_size, VK_NULL_HANDLE);
}

void PerFrameResourcesStorage::requestTargetMipLevel(RenderTarget target, uint32_t level)
{
	if (target == RenderTarget::Swapchain && level != 0) {
		Log::error("Requested MIP level #{} for swapchain render target (only #0 is allowed)", level);
		throw MessageException("invalid usage of swapchain render target");
	}

	auto iter = m_info.find(target);
	if (iter == m_info.end()) {
		Log::error("Requested target '{}' does not exist in this static configuration", getTargetName(target));
		throw MessageException("access to non-existing render target");
	}

	iter->second.num_mip_levels = std::max(iter->second.num_mip_levels, level + 1);
}

void PerFrameResourcesStorage::requestTargetArrayLayer(RenderTarget target, uint32_t layer)
{
	if (target == RenderTarget::Swapchain && layer != 0) {
		Log::error("Requested array layer #{} for swapchain render target (only #0 is allowed)", layer);
		throw MessageException("invalid usage of swapchain render target");
	}

	auto iter = m_info.find(target);
	if (iter == m_info.end()) {
		Log::error("Requested target '{}' does not exist in this static configuration", getTargetName(target));
		throw MessageException("access to non-existing render target");
	}

	iter->second.num_array_layers = std::max(iter->second.num_array_layers, layer + 1);
}

void PerFrameResourcesStorage::requestTargetUsage(RenderTarget target, VkImageUsageFlags usage)
{
	// TODO: check limitations for swapchain images?

	auto iter = m_info.find(target);
	if (iter == m_info.end()) {
		Log::error("Requested target '{}' does not exist in this static configuration", getTargetName(target));
		throw MessageException("access to non-existing render target");
	}

	iter->second.create_usage |= usage;
}

void PerFrameResourcesStorage::createResources(const GraphicsOptions &opts)
{
	if (m_created_resources) {
		Log::error("Attempted double call to 'createResources'. This is a bug in Voxen.");
		throw MessageException("double call to createResources");
	}
	m_created_resources = true;

	auto resolve_format = [](RenderTargetFormatClass) -> VkFormat {
		return VK_FORMAT_UNDEFINED;
	};

	const VkExtent3D window_dimensions {
		.width = uint32_t(opts.frameSize().first),
		.height = uint32_t(opts.frameSize().second),
		.depth = 1
	};
	const VkExtent3D scene_dimensions {
		.width = uint32_t(opts.frameSize().first * opts.sceneResolutionScale() + 0.5f),
		.height = uint32_t(opts.frameSize().second * opts.sceneResolutionScale() + 0.5f),
		.depth = 1
	};

	auto resolve_dimensions = [&](RenderTargetDimensionsClass dimensions) -> VkExtent3D {
		switch (dimensions) {
		case RenderTargetDimensionsClass::Window: return window_dimensions;
		case RenderTargetDimensionsClass::Scene: return scene_dimensions;
		default:
			Log::error("Unknown dimensions class met. This is a bug in Voxen.");
			throw MessageException("unknown dimensions class");
		}
	};

	VkSampleCountFlagBits aa_method_samples;

	switch (opts.aaMethod()) {
	case GraphicsOptions::AaMethod::None: [[fallthrough]];
	case GraphicsOptions::AaMethod::Taa1S2T: [[fallthrough]];
	case GraphicsOptions::AaMethod::Taa1S4T: [[fallthrough]];
	case GraphicsOptions::AaMethod::Taa1S8T:
		aa_method_samples = VK_SAMPLE_COUNT_1_BIT;
		break;
	case GraphicsOptions::AaMethod::Msaa2x:
		aa_method_samples = VK_SAMPLE_COUNT_2_BIT;
		break;
	case GraphicsOptions::AaMethod::Msaa4x:
		aa_method_samples = VK_SAMPLE_COUNT_4_BIT;
		break;
	case GraphicsOptions::AaMethod::Msaa8x:
		aa_method_samples = VK_SAMPLE_COUNT_8_BIT;
		break;
	default:
		Log::error("Unknown AA method is selected. This is a bug in Voxen.");
		throw MessageException("unknown AA method");
	}

	auto resolve_samples = [&](RenderTargetSamplesClass samples) -> VkSampleCountFlagBits {
		switch (samples) {
		case RenderTargetSamplesClass::One: return VK_SAMPLE_COUNT_1_BIT;
		case RenderTargetSamplesClass::ByAaMethod: return aa_method_samples;
		default:
			Log::error("Unknown samples class met. This is a bug in Voxen.");
			throw MessageException("unknown samples class");
		}
	};

	auto &backend = Backend::backend();
	VkDevice device = backend.device();
	//auto &allocator = backend.deviceAllocator();

	for (auto &[target, info] : m_info) {
		info.create_format = resolve_format(info.format);
		info.create_extent = resolve_dimensions(info.dimensions);
		info.create_samples = resolve_samples(info.samples);

		if (info.num_mip_levels == 0 || info.num_array_layers == 0 || info.create_extent.width == 0 ||
			info.create_extent.height == 0 || info.create_extent.depth == 0) {
			// This target is unused, don't need to create any resource
			continue;
		}

		if (target == RenderTarget::Swapchain) {
			// Swapchain images will be provided externally on `advanceTemporalCopies`
			continue;
		}

		VkImageCreateInfo image_info{};
		image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_info.flags = info.create_flags;
		image_info.imageType = info.create_type;
		image_info.format = info.create_format;
		image_info.extent = info.create_extent;
		image_info.mipLevels = info.num_mip_levels;
		image_info.arrayLayers = info.num_array_layers;
		image_info.samples = info.create_samples;
		image_info.tiling = info.create_tiling;
		image_info.usage = info.create_usage;
		image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		for (VkImage &image : info.temporal_copies) {
			VkResult result = backend.vkCreateImage(device, &image_info, VulkanHostAllocator::callbacks(), &image);
			if (result != VK_SUCCESS) {
				throw VulkanException(result, "vkCreateImage");
			}

			// TODO: allocate and bind memory
		}
	}
}

void PerFrameResourcesStorage::advanceTemporalCopies(VkImage swapchain_image)
{
	for (auto &[target, info] : m_info) {
		if (target == RenderTarget::Swapchain) {
			info.temporal_copies[0] = swapchain_image;
		} else {
			info.cur_temporal_copy = (info.cur_temporal_copy + 1) % uint32_t(info.temporal_copies.size());
		}
	}
}

}
