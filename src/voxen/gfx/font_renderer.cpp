#include <voxen/gfx/font_renderer.hpp>

#include <voxen/common/assets/png_tools.hpp>
#include <voxen/common/filemanager.hpp>
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

constexpr int32_t FONT_ATLAS_WIDTH = 408;
constexpr int32_t FONT_ATLAS_HEIGHT = 408;

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
	{ ' ', 17, 25, 0, 0, 0, 0, 26 },
	{ '!', 43, 6, 15, 38, 5, 34, 26 },
	{ '"', 73, 14, 24, 23, 1, 34, 26 },
	{ '#', 105, 8, 28, 35, -1, 31, 26 },
	{ '$', 139, 3, 27, 45, -1, 36, 26 },
	{ '%', 171, 8, 32, 35, -3, 31, 26 },
	{ '&', 205, 7, 32, 36, -3, 32, 26 },
	{ '\'', 248, 14, 14, 23, 6, 34, 26 },
	{ '(', 278, 2, 21, 47, 4, 35, 26 },
	{ ')', 313, 2, 20, 47, 1, 35, 26 },
	{ '*', 343, 12, 28, 27, -1, 28, 26 },
	{ '+', 377, 11, 27, 28, -1, 28, 26 },
	{ ',', 8, 64, 17, 24, 4, 11, 26 },
	{ '-', 37, 71, 27, 11, -1, 20, 26 },
	{ '.', 77, 68, 16, 16, 5, 12, 26 },
	{ '/', 106, 53, 26, 46, 0, 35, 26 },
	{ '0', 139, 59, 28, 35, -1, 31, 26 },
	{ '1', 173, 59, 27, 35, 0, 31, 26 },
	{ '2', 206, 59, 29, 35, -2, 31, 26 },
	{ '3', 240, 59, 29, 35, -2, 31, 26 },
	{ '4', 273, 59, 31, 35, -3, 31, 26 },
	{ '5', 308, 59, 29, 35, -2, 31, 26 },
	{ '6', 343, 59, 28, 35, -1, 31, 26 },
	{ '7', 376, 59, 29, 35, -2, 31, 26 },
	{ '8', 2, 110, 29, 35, -2, 31, 26 },
	{ '9', 36, 110, 29, 35, -2, 31, 26 },
	{ ':', 77, 112, 16, 31, 5, 27, 26 },
	{ ';', 110, 108, 17, 39, 4, 26, 26 },
	{ '<', 140, 111, 25, 33, 1, 31, 26 },
	{ '=', 173, 117, 27, 20, -1, 24, 26 },
	{ '>', 208, 111, 25, 33, 0, 31, 26 },
	{ '?', 242, 108, 25, 38, 0, 34, 26 },
	{ '@', 274, 107, 30, 41, -2, 31, 26 },
	{ 'A', 307, 109, 32, 36, -3, 32, 26 },
	{ 'B', 343, 109, 28, 36, 0, 32, 26 },
	{ 'C', 376, 109, 30, 36, -2, 32, 26 },
	{ 'D', 2, 160, 29, 36, -1, 32, 26 },
	{ 'E', 37, 160, 27, 36, 0, 32, 26 },
	{ 'F', 72, 160, 26, 36, 1, 32, 26 },
	{ 'G', 104, 160, 29, 36, -2, 32, 26 },
	{ 'H', 139, 160, 28, 36, -1, 32, 26 },
	{ 'I', 174, 160, 26, 36, 0, 32, 26 },
	{ 'J', 207, 160, 27, 36, -1, 32, 26 },
	{ 'K', 240, 160, 29, 36, 0, 32, 26 },
	{ 'L', 275, 160, 27, 36, 1, 32, 26 },
	{ 'M', 309, 160, 28, 36, -1, 32, 26 },
	{ 'N', 343, 160, 28, 36, -1, 32, 26 },
	{ 'O', 376, 160, 30, 36, -2, 32, 26 },
	{ 'P', 3, 211, 28, 36, 0, 32, 26 },
	{ 'Q', 36, 208, 30, 43, -2, 32, 26 },
	{ 'R', 71, 211, 28, 36, 0, 32, 26 },
	{ 'S', 104, 211, 29, 36, -2, 32, 26 },
	{ 'T', 137, 211, 31, 36, -3, 32, 26 },
	{ 'U', 173, 211, 28, 36, -1, 32, 26 },
	{ 'V', 205, 211, 31, 36, -3, 32, 26 },
	{ 'W', 238, 211, 34, 36, -4, 32, 26 },
	{ 'X', 274, 211, 30, 36, -2, 32, 26 },
	{ 'Y', 307, 211, 31, 36, -3, 32, 26 },
	{ 'Z', 342, 211, 29, 36, -2, 32, 26 },
	{ '[', 380, 207, 21, 45, 5, 34, 26 },
	{ '\\', 4, 257, 26, 46, 0, 35, 26 },
	{ ']', 41, 258, 20, 45, 0, 34, 26 },
	{ '^', 72, 267, 25, 26, 0, 33, 26 },
	{ '_', 104, 274, 30, 12, -2, 2, 26 },
	{ '`', 144, 271, 18, 18, 2, 38, 26 },
	{ 'a', 173, 265, 27, 30, -1, 25, 26 },
	{ 'b', 206, 260, 29, 40, -1, 35, 26 },
	{ 'c', 241, 266, 28, 29, -1, 25, 26 },
	{ 'd', 275, 260, 28, 40, -2, 35, 26 },
	{ 'e', 308, 266, 29, 29, -2, 25, 26 },
	{ 'f', 342, 261, 29, 39, 0, 35, 26 },
	{ 'g', 376, 261, 30, 38, -1, 25, 26 },
	{ 'h', 3, 312, 28, 39, -1, 35, 26 },
	{ 'i', 39, 312, 23, 39, -1, 35, 26 },
	{ 'j', 73, 307, 24, 48, -2, 35, 26 },
	{ 'k', 104, 312, 29, 39, 0, 35, 26 },
	{ 'l', 139, 312, 28, 39, -1, 35, 26 },
	{ 'm', 172, 317, 30, 29, -2, 25, 26 },
	{ 'n', 207, 317, 28, 29, -1, 25, 26 },
	{ 'o', 240, 317, 30, 29, -2, 25, 26 },
	{ 'p', 274, 312, 29, 38, -1, 25, 26 },
	{ 'q', 309, 312, 28, 38, -2, 25, 26 },
	{ 'r', 344, 317, 25, 29, 2, 25, 26 },
	{ 's', 377, 317, 28, 29, -1, 25, 26 },
	{ 't', 2, 364, 30, 36, -2, 32, 26 },
	{ 'u', 37, 368, 27, 29, -1, 25, 26 },
	{ 'v', 70, 368, 30, 29, -2, 25, 26 },
	{ 'w', 102, 368, 34, 29, -4, 25, 26 },
	{ 'x', 138, 368, 29, 29, -2, 25, 26 },
	{ 'y', 172, 363, 30, 38, -2, 25, 26 },
	{ 'z', 207, 368, 28, 29, -1, 25, 26 },
	{ '{', 242, 360, 25, 45, 1, 34, 26 },
	{ '|', 283, 357, 12, 51, 7, 36, 26 },
	{ '}', 310, 360, 25, 45, 0, 34, 26 },
	{ '~', 343, 374, 28, 16, -1, 22, 26 },
};

} // namespace

struct FontRenderer::Resources {
	VkImage atlas_handle = VK_NULL_HANDLE;
	VmaAllocation atlas_alloc = VK_NULL_HANDLE;
	VkImageView atlas_srv = VK_NULL_HANDLE;
	VkSampler atlas_sampler = VK_NULL_HANDLE;
};

FontRenderer::FontRenderer(GfxSystem &gfx) : m_gfx(gfx), m_resources(std::make_unique<Resources>()) {}

FontRenderer::~FontRenderer()
{
	if (m_resources->atlas_handle) {
		vk::Device &dev = *m_gfx.device();

		dev.enqueueDestroy(m_resources->atlas_sampler);
		dev.enqueueDestroy(m_resources->atlas_srv);
		dev.enqueueDestroy(m_resources->atlas_handle, m_resources->atlas_alloc);
	}
}

void FontRenderer::loadResources()
{
	createFontAtlasTexture();
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
	// TODO: don't hardcode this so hard
	auto maybe_png_array = FileManager::readFile("assets/fonts/ascii.png");
	if (!maybe_png_array.has_value()) {
		Log::error("Can't load font atlas PNG");
		throw Exception::fromError(VoxenErrc::InvalidData, "font atlas read failed");
	}

	auto png_bytes = maybe_png_array->as_bytes();

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
