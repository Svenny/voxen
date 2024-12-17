#include <voxen/gfx/font_renderer.hpp>

#include <voxen/common/assets/png_tools.hpp>
#include <voxen/gfx/gfx_system.hpp>
#include <voxen/gfx/vk/vk_command_allocator.hpp>
#include <voxen/gfx/vk/vk_device.hpp>
#include <voxen/gfx/vk/vk_error.hpp>
#include <voxen/gfx/vk/vk_transient_buffer_allocator.hpp>
#include <voxen/util/error_condition.hpp>
#include <voxen/util/exception.hpp>
#include <voxen/util/log.hpp>

// TODO: legacy code
#include <voxen/client/vulkan/backend.hpp>
#include <voxen/client/vulkan/pipeline.hpp>
#include <voxen/client/vulkan/pipeline_layout.hpp>

#include "font_atlas_blob.hpp"

#include <glm/gtc/packing.hpp>

#include <vma/vk_mem_alloc.h>

namespace voxen::gfx
{

namespace
{

constexpr float FONT_ASCENT = 15.375f;
constexpr float FONT_DESCENT = -4.26562f;
constexpr float FONT_LINEHEIGHT = 19.6406f;

constexpr char MIN_RENDERABLE_CHAR = ' ';
constexpr char MAX_RENDERABLE_CHAR = '~';

constexpr int32_t FONT_ATLAS_WIDTH = 512;
constexpr int32_t FONT_ATLAS_HEIGHT = 384;

constexpr struct GlyphInfo {
	char c;
	float atlas_x;
	float atlas_y;
	float width;
	float height;
	float bearing_x;
	float bearing_y;
	float advance_x;
} GLYPH_INFOS[] = {
	{ ' ', 16, 32, 0, 0, 0, 0, 26 },
	{ '!', 41, 15, 14, 34, 6, 31, 26 },
	{ '"', 69, 22, 22, 20, 2, 32, 26 },
	{ '#', 99, 15, 26, 33, 0, 30, 26 },
	{ '$', 131, 10, 26, 44, 0, 36, 26 },
	{ '%', 161, 15, 30, 34, -2, 31, 26 },
	{ '&', 193, 15, 30, 34, -2, 31, 26 },
	{ '\'', 234, 22, 12, 20, 7, 32, 26 },
	{ '(', 262, 9, 19, 45, 5, 34, 26 },
	{ ')', 295, 9, 18, 45, 2, 34, 26 },
	{ '*', 323, 19, 26, 26, 0, 28, 26 },
	{ '+', 355, 18, 26, 27, 0, 28, 26 },
	{ ',', 392, 20, 16, 23, 5, 10, 26 },
	{ '-', 419, 27, 26, 9, 0, 19, 26 },
	{ '.', 457, 25, 14, 13, 6, 10, 26 },
	{ '/', 484, 10, 24, 43, 1, 34, 26 },
	{ '0', 3, 79, 26, 33, 0, 30, 26 },
	{ '1', 35, 79, 25, 33, 1, 30, 26 },
	{ '2', 66, 79, 27, 33, -1, 30, 26 },
	{ '3', 98, 79, 27, 33, -1, 30, 26 },
	{ '4', 129, 79, 29, 33, -2, 30, 26 },
	{ '5', 162, 79, 27, 33, -1, 30, 26 },
	{ '6', 195, 79, 26, 33, 0, 30, 26 },
	{ '7', 227, 79, 26, 33, 0, 30, 26 },
	{ '8', 258, 79, 27, 33, -1, 30, 26 },
	{ '9', 290, 79, 27, 33, -1, 30, 26 },
	{ ':', 329, 82, 14, 28, 6, 25, 26 },
	{ ';', 360, 77, 16, 38, 5, 25, 26 },
	{ '<', 388, 81, 23, 29, 2, 29, 26 },
	{ '=', 419, 86, 26, 19, 0, 24, 26 },
	{ '>', 452, 81, 23, 29, 1, 29, 26 },
	{ '?', 484, 78, 23, 35, 1, 32, 26 },
	{ '@', 2, 139, 28, 41, -1, 31, 26 },
	{ 'A', 33, 143, 30, 34, -2, 31, 26 },
	{ 'B', 67, 143, 26, 34, 1, 31, 26 },
	{ 'C', 98, 143, 28, 34, -1, 31, 26 },
	{ 'D', 130, 143, 27, 34, 0, 31, 26 },
	{ 'E', 163, 143, 25, 34, 1, 31, 26 },
	{ 'F', 195, 143, 25, 34, 2, 31, 26 },
	{ 'G', 226, 143, 27, 34, -1, 31, 26 },
	{ 'H', 259, 143, 26, 34, 0, 31, 26 },
	{ 'I', 292, 143, 24, 34, 1, 31, 26 },
	{ 'J', 323, 143, 25, 34, 0, 31, 26 },
	{ 'K', 354, 143, 27, 34, 1, 31, 26 },
	{ 'L', 387, 143, 25, 34, 2, 31, 26 },
	{ 'M', 419, 143, 26, 34, 0, 31, 26 },
	{ 'N', 451, 143, 26, 34, 0, 31, 26 },
	{ 'O', 482, 143, 28, 34, -1, 31, 26 },
	{ 'P', 3, 207, 26, 34, 1, 31, 26 },
	{ 'Q', 34, 203, 28, 41, -1, 31, 26 },
	{ 'R', 67, 207, 26, 34, 1, 31, 26 },
	{ 'S', 98, 207, 28, 34, -1, 31, 26 },
	{ 'T', 129, 207, 29, 34, -2, 31, 26 },
	{ 'U', 163, 207, 26, 34, 0, 31, 26 },
	{ 'V', 193, 207, 29, 34, -2, 31, 26 },
	{ 'W', 224, 207, 32, 34, -3, 31, 26 },
	{ 'X', 258, 207, 28, 34, -1, 31, 26 },
	{ 'Y', 289, 207, 30, 34, -2, 31, 26 },
	{ 'Z', 322, 207, 28, 34, -1, 31, 26 },
	{ '[', 358, 202, 19, 44, 6, 34, 26 },
	{ '\\', 388, 202, 24, 43, 1, 34, 26 },
	{ ']', 422, 202, 19, 44, 1, 34, 26 },
	{ '^', 452, 212, 24, 23, 1, 31, 26 },
	{ '_', 482, 219, 28, 9, -1, 0, 26 },
	{ '`', 8, 280, 15, 15, 4, 36, 26 },
	{ 'a', 35, 274, 26, 27, 0, 24, 26 },
	{ 'b', 67, 269, 26, 37, 1, 34, 26 },
	{ 'c', 98, 274, 27, 27, 0, 24, 26 },
	{ 'd', 131, 269, 26, 37, -1, 34, 26 },
	{ 'e', 162, 274, 28, 27, -1, 24, 26 },
	{ 'f', 194, 269, 27, 37, 1, 34, 26 },
	{ 'g', 226, 270, 28, 36, 0, 24, 26 },
	{ 'h', 259, 269, 25, 37, 1, 34, 26 },
	{ 'i', 293, 269, 21, 37, 0, 34, 26 },
	{ 'j', 325, 265, 22, 46, -1, 34, 26 },
	{ 'k', 354, 269, 27, 37, 1, 34, 26 },
	{ 'l', 386, 269, 27, 37, 0, 34, 26 },
	{ 'm', 418, 274, 28, 27, -1, 24, 26 },
	{ 'n', 451, 274, 25, 27, 1, 24, 26 },
	{ 'o', 482, 274, 28, 27, -1, 24, 26 },
	{ 'p', 3, 334, 26, 36, 1, 24, 26 },
	{ 'q', 35, 334, 26, 36, -1, 24, 26 },
	{ 'r', 68, 338, 24, 27, 3, 24, 26 },
	{ 's', 99, 338, 26, 27, 0, 24, 26 },
	{ 't', 130, 335, 28, 34, -1, 31, 26 },
	{ 'u', 163, 338, 25, 27, 0, 24, 26 },
	{ 'v', 194, 338, 28, 27, -1, 24, 26 },
	{ 'w', 224, 338, 32, 27, -3, 24, 26 },
	{ 'x', 258, 338, 27, 27, -1, 24, 26 },
	{ 'y', 290, 334, 28, 36, -1, 24, 26 },
	{ 'z', 323, 338, 26, 27, 0, 24, 26 },
	{ '{', 356, 330, 23, 44, 2, 34, 26 },
	{ '|', 395, 327, 10, 49, 8, 35, 26 },
	{ '}', 420, 330, 23, 44, 1, 34, 26 },
	{ '~', 451, 345, 26, 14, 0, 21, 26 },
};

} // namespace

struct FontRenderer::Resources {
	VkImage atlas_handle = VK_NULL_HANDLE;
	VmaAllocation atlas_alloc = VK_NULL_HANDLE;
	VkImageView atlas_srv = VK_NULL_HANDLE;
	VkSampler atlas_sampler = VK_NULL_HANDLE;
};

FontRenderer::FontRenderer(GfxSystem &gfx) : m_gfx(gfx), m_resources(std::make_unique<Resources>())
{
	createFontAtlasTexture();
}

FontRenderer::~FontRenderer()
{
	if (m_resources->atlas_handle) {
		vk::Device &dev = *m_gfx.device();

		dev.enqueueDestroy(m_resources->atlas_sampler);
		dev.enqueueDestroy(m_resources->atlas_srv);
		dev.enqueueDestroy(m_resources->atlas_handle, m_resources->atlas_alloc);
	}
}

std::vector<FontRenderer::GlyphCommand> FontRenderer::getGlyphCommands(std::span<const TextItem> text_items)
{
	std::vector<GlyphCommand> cmds;

	for (const TextItem &item : text_items) {
		const glm::u8vec4 color_srgb(item.color.r, item.color.g, item.color.b, item.color.a);

		glm::vec2 screen_pos = item.origin_screen;

		for (char c : item.text) {
			if (c == '\n') {
				// New line
				screen_pos.x = item.origin_screen.x;
				screen_pos.y += (FONT_ASCENT - FONT_DESCENT + FONT_LINEHEIGHT) * m_font_scaling;
			}

			if (c < MIN_RENDERABLE_CHAR || c > MAX_RENDERABLE_CHAR) {
				// TODO: warn about non-renderable text?
				continue;
			}

			constexpr glm::vec2 UV_SCALER = 1.0f / glm::vec2(FONT_ATLAS_WIDTH, FONT_ATLAS_HEIGHT);
			const GlyphInfo &info = GLYPH_INFOS[c - MIN_RENDERABLE_CHAR];

			GlyphCommand &cmd = cmds.emplace_back();
			cmd.up_left_pos = screen_pos + glm::vec2(info.bearing_x, FONT_ASCENT - info.bearing_y) * m_font_scaling;
			cmd.lo_right_pos = cmd.up_left_pos + glm::vec2(info.width, info.height) * m_font_scaling;
			cmd.up_left_uv = glm::vec2(info.atlas_x, info.atlas_y) * UV_SCALER;
			cmd.lo_right_uv = glm::vec2(info.atlas_x + info.width, info.atlas_y + info.height) * UV_SCALER;
			cmd.color_srgb = color_srgb;

			screen_pos.x += info.advance_x * m_font_scaling;
		}
	}

	return cmds;
}

void FontRenderer::drawUi(VkCommandBuffer cmd_buf, std::span<const TextItem> text_items, glm::vec2 inv_screen_size)
{
	auto font_cmds = getGlyphCommands(text_items);

	vk::Device &dev = *m_gfx.device();
	auto &ddt = dev.dt();

	size_t cmds_size = sizeof(GlyphCommand) * font_cmds.size();

	auto alloc = m_gfx.transientBufferAllocator()->allocate(vk::TransientBufferAllocator::TypeUpload, cmds_size, 64);
	memcpy(alloc.host_pointer, font_cmds.data(), cmds_size);

	VkDescriptorImageInfo atlas_info {
		.sampler = m_resources->atlas_sampler,
		.imageView = m_resources->atlas_srv,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	VkDescriptorBufferInfo cbuf_info {
		.buffer = alloc.buffer,
		.offset = alloc.buffer_offset,
		.range = cmds_size,
	};

	VkWriteDescriptorSet descriptors[2] = {
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = VK_NULL_HANDLE,
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &atlas_info,
			.pBufferInfo = nullptr,
			.pTexelBufferView = nullptr,
		},
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = VK_NULL_HANDLE,
			.dstBinding = 1,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pImageInfo = nullptr,
			.pBufferInfo = &cbuf_info,
			.pTexelBufferView = nullptr,
		},
	};

	auto &legacy_backend = client::vulkan::Backend::backend();
	auto &pipeline_layout = legacy_backend.pipelineLayoutCollection().uiFontLayout();

	ddt.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
		legacy_backend.pipelineCollection()[client::vulkan::PipelineCollection::UI_FONT_PIPELINE]);
	ddt.vkCmdPushConstants(cmd_buf, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::vec2),
		glm::value_ptr(inv_screen_size));
	ddt.vkCmdPushDescriptorSetKHR(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, std::size(descriptors),
		descriptors);
	ddt.vkCmdDraw(cmd_buf, 6, uint32_t(font_cmds.size()), 0, 0);
}

void FontRenderer::createFontAtlasTexture()
{
	auto png_bytes = std::as_bytes(std::span<const uint8_t>(FONT_ATLAS_STORAGE, FONT_ATLAS_STORAGE_SIZE));

	assets::PngInfo png_info;
	auto unpacked_bitmap = assets::PngTools::unpack(png_bytes, png_info, false);

	if (unpacked_bitmap.empty()) {
		Log::error("Can't unpack font atlas PNG");
		throw Exception::fromError(VoxenErrc::InvalidData, "font unpacking failed");
	}

	if (png_info.resolution.width != FONT_ATLAS_WIDTH || png_info.resolution.height != FONT_ATLAS_HEIGHT) {
		Log::error("Font atlas PNG has unpexpected dimensions {}x{} (expected {}x{})", png_info.resolution.width,
			png_info.resolution.height, FONT_ATLAS_WIDTH, FONT_ATLAS_HEIGHT);
		throw Exception::fromError(VoxenErrc::InvalidData, "font atlas has unexpected dimensions");
	}

	if (png_info.is_16bpc || png_info.channels != 1) {
		Log::error("Font atlas PNG has unexpected properties ({}, {} channels) - expected (8 bpc, 1 channel)",
			png_info.is_16bpc ? 16 : 8, png_info.channels);
		throw Exception::fromError(VoxenErrc::InvalidData, "font atlas has unexpected properties");
	}

	VkImageCreateInfo image_create_info {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_R8_UNORM,
		.extent = { .width = FONT_ATLAS_WIDTH, .height = FONT_ATLAS_HEIGHT, .depth = 1 },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	VmaAllocationCreateInfo alloc_create_info {};
	alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO;

	vk::Device &dev = *m_gfx.device();

	VkResult res = vmaCreateImage(dev.vma(), &image_create_info, &alloc_create_info, &m_resources->atlas_handle,
		&m_resources->atlas_alloc, nullptr);
	if (res != VK_SUCCESS) {
		throw vk::VulkanException(res, "vmaCreateImage");
	}

	dev.setObjectName(m_resources->atlas_handle, "gfx/font/atlas");

	VkImageViewUsageCreateInfo srv_usage_info {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO,
		.pNext = nullptr,
		.usage = VK_IMAGE_USAGE_SAMPLED_BIT,
	};

	VkImageViewCreateInfo srv_create_info
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = &srv_usage_info,
		.flags = 0,
		.image = m_resources->atlas_handle,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = VK_FORMAT_R8_UNORM,
		.components = {
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
		},
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	m_resources->atlas_srv = dev.vkCreateImageView(srv_create_info, "gfx/font/atlas/srv");

	VkSamplerCreateInfo sampler_create_info {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.mipLodBias = 0.0f,
		.anisotropyEnable = VK_FALSE,
		.maxAnisotropy = 0.0f,
		.compareEnable = VK_FALSE,
		.compareOp = VK_COMPARE_OP_ALWAYS,
		.minLod = 0.0f,
		.maxLod = VK_LOD_CLAMP_NONE,
		.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
		.unnormalizedCoordinates = VK_FALSE,
	};

	m_resources->atlas_sampler = dev.vkCreateSampler(sampler_create_info, "gfx/font/atlas/sampler");

	VkCommandBuffer cmd_buf = m_gfx.commandAllocator()->allocate(vk::Device::QueueMain);

	VkCommandBufferBeginInfo cmd_begin_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = nullptr,
	};

	res = dev.dt().vkBeginCommandBuffer(cmd_buf, &cmd_begin_info);
	if (res != VK_SUCCESS) {
		throw vk::VulkanException(res, "vkBeginCommandBuffer");
	}

	const size_t bitmap_size = unpacked_bitmap.size();
	auto staging = m_gfx.transientBufferAllocator()->allocate(vk::TransientBufferAllocator::TypeUpload, bitmap_size, 4);
	memcpy(staging.host_pointer, unpacked_bitmap.data(), bitmap_size);

	VkBufferImageCopy copy_region = {};
	copy_region.bufferOffset = staging.buffer_offset;
	copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy_region.imageSubresource.layerCount = 1;
	copy_region.imageExtent = { .width = FONT_ATLAS_WIDTH, .height = FONT_ATLAS_HEIGHT, .depth = 1 };

	VkImageMemoryBarrier2 pre_image_barrier {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.pNext = nullptr,
		.srcStageMask = 0,
		.srcAccessMask = 0,
		.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
		.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = m_resources->atlas_handle,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	VkImageMemoryBarrier2 post_image_barrier {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.pNext = nullptr,
		.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
		.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
		.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = m_resources->atlas_handle,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	VkDependencyInfo pre_barrier {};
	pre_barrier.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	pre_barrier.imageMemoryBarrierCount = 1;
	pre_barrier.pImageMemoryBarriers = &pre_image_barrier;

	VkDependencyInfo post_barrier {};
	post_barrier.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	post_barrier.imageMemoryBarrierCount = 1;
	post_barrier.pImageMemoryBarriers = &post_image_barrier;

	dev.dt().vkCmdPipelineBarrier2(cmd_buf, &pre_barrier);
	dev.dt().vkCmdCopyBufferToImage(cmd_buf, staging.buffer, m_resources->atlas_handle,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);
	dev.dt().vkCmdPipelineBarrier2(cmd_buf, &post_barrier);

	res = dev.dt().vkEndCommandBuffer(cmd_buf);
	if (res != VK_SUCCESS) {
		throw vk::VulkanException(res, "vkEndCommandBuffer");
	}

	dev.submitCommands(vk::Device::SubmitInfo {
		.queue = vk::Device::QueueMain,
		.cmds = std::span(&cmd_buf, 1),
	});
}

} // namespace voxen::gfx
