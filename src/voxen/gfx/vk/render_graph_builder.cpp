#include <voxen/gfx/vk/render_graph_builder.hpp>

#include "render_graph_private.hpp"

namespace voxen::gfx::vk
{

namespace
{

void initImageCreateInfo(VkImageCreateInfo &ci, const RenderGraphBuilder::Image2DConfig &config)
{
	ci = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = config.format,
		.extent = { uint32_t(config.resolution.width), uint32_t(config.resolution.height), 1 },
		.mipLevels = config.mips,
		.arrayLayers = config.layers,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = 0,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};
}

void initBufferCreateInfo(VkBufferCreateInfo &ci, VkDeviceSize size)
{
	ci = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.size = size,
		.usage = 0,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
	};
}

void initImageViewCreateInfo(VkImageViewCreateInfo &ci, VkImageViewUsageCreateInfo &uci, VkImageViewType type,
	VkFormat format, VkImageSubresourceRange range)
{
	ci = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = &uci,
		.flags = 0,
		.image = VK_NULL_HANDLE,
		.viewType = type,
		.format = format,
		.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY },
		.subresourceRange = range,
	};

	uci = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO,
		.pNext = nullptr,
		.usage = 0,
	};
}

VkImageAspectFlags formatToAspect(VkFormat format)
{
	if (format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D16_UNORM) {
		return VK_IMAGE_ASPECT_DEPTH_BIT;
	} else if (format == VK_FORMAT_S8_UINT) {
		return VK_IMAGE_ASPECT_STENCIL_BIT;
	} else if (format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT) {
		return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	return VK_IMAGE_ASPECT_COLOR_BIT;
}

void splitReadWriteFlags(VkAccessFlags2 flags, VkAccessFlags2 &read_flags, VkAccessFlags2 &write_flags)
{
	constexpr VkAccessFlags2 KNOWN_READ_FLAGS = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_UNIFORM_READ_BIT
		| VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT
		| VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT;

	constexpr VkAccessFlags2 KNOWN_WRITE_FLAGS = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
		| VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT
		| VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;

	// Ensure no unknown flags are passed
	assert((flags & (KNOWN_READ_FLAGS | KNOWN_WRITE_FLAGS)) == flags);

	read_flags = flags & KNOWN_READ_FLAGS;
	write_flags = flags & KNOWN_WRITE_FLAGS;
}

VkBufferUsageFlags bufferAccessToUsage(VkAccessFlags2 flags)
{
	VkBufferUsageFlags usage = 0;

	constexpr std::pair<VkAccessFlags2, VkBufferUsageFlags> mapping[] = {
		{ VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT },
		{ VK_ACCESS_2_UNIFORM_READ_BIT, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT },
		{ VK_ACCESS_2_TRANSFER_READ_BIT, VK_BUFFER_USAGE_TRANSFER_SRC_BIT },
		{ VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_BUFFER_USAGE_TRANSFER_DST_BIT },
		{ VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT },
		{ VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT },
	};

	for (const auto &entry : mapping) {
		if (flags & entry.first) {
			flags &= ~entry.first;
			usage |= entry.second;
		}
	}

	// Ensure no unknown flags remain
	assert(!flags);

	return usage;
}

VkImageUsageFlags imageAccessToUsage(VkAccessFlags2 flags)
{
	VkImageUsageFlags usage = 0;

	constexpr std::pair<VkAccessFlags2, VkImageUsageFlags> mapping[] = {
		{ VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT },
		{ VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT },
		{ VK_ACCESS_2_TRANSFER_READ_BIT, VK_IMAGE_USAGE_TRANSFER_SRC_BIT },
		{ VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT },
		{ VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_IMAGE_USAGE_SAMPLED_BIT },
		{ VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_IMAGE_USAGE_STORAGE_BIT },
	};

	for (const auto &entry : mapping) {
		if (flags & entry.first) {
			flags &= ~entry.first;
			usage |= entry.second;
		}
	}

	// Ensure no unknown flags remain
	assert(!flags);

	return usage;
}

} // namespace

RenderGraphBuilder::RenderGraphBuilder(RenderGraphPrivate &priv) noexcept : m_private(priv)
{
	// TODO: stub before render graph is integrated with swapchain
	m_private.output_image = make2DImage("output_stub",
		{
			.format = VK_FORMAT_R8G8B8A8_SRGB,
			.resolution = { 1280, 720 },
			.mips = 1,
			.layers = 1,
		});
}

RenderGraphBuilder::~RenderGraphBuilder() noexcept = default;

Device &RenderGraphBuilder::device() noexcept
{
	return m_private.device;
}

RenderGraphImage &RenderGraphBuilder::outputImage()
{
	return m_private.output_image;
}

RenderGraphImage RenderGraphBuilder::make2DImage(std::string_view name, Image2DConfig config)
{
	auto &priv = m_private.images.emplace_back();

	priv.name = fmt::format("graph/img/{}", name);
	priv.mip_states.resize(config.mips);
	initImageCreateInfo(priv.create_info, config);

	return RenderGraphImage(priv);
}

std::pair<RenderGraphImage, RenderGraphImage> RenderGraphBuilder::makeDoubleBuffered2DImage(std::string_view name,
	Image2DConfig config)
{
	auto &priv = m_private.images.emplace_back();
	auto &prev_priv = m_private.images.emplace_back();

	priv.name = fmt::format("graph/img/{}@A", name);
	priv.temporal_sibling = &prev_priv;
	priv.mip_states.resize(config.mips);
	initImageCreateInfo(priv.create_info, config);

	prev_priv.name = fmt::format("graph/img/{}@B", name);
	prev_priv.temporal_sibling = &priv;
	prev_priv.mip_states.resize(config.mips);
	initImageCreateInfo(prev_priv.create_info, config);

	return { RenderGraphImage(priv), RenderGraphImage(prev_priv) };
}

RenderGraphBuffer RenderGraphBuilder::makeBuffer(std::string_view name, BufferConfig config)
{
	auto &priv = m_private.buffers.emplace_back();

	priv.name = fmt::format("graph/buf/{}", name);
	priv.dynamic_sized = config.dynamic_size;
	initBufferCreateInfo(priv.create_info, config.size);

	return RenderGraphBuffer(priv);
}

RenderGraphImageView RenderGraphBuilder::makeBasicImageView(std::string_view name, RenderGraphImage &image)
{
	auto *image_priv = image.getPrivate();
	if (!image_priv) {
		return {};
	}

	return makeImageView(name, image, image_priv->create_info.format);
}

RenderGraphImageView RenderGraphBuilder::makeSingleMipImageView(std::string_view name, RenderGraphImage &image,
	uint32_t mip)
{
	auto *image_priv = image.getPrivate();
	if (!image_priv) {
		return {};
	}

	return makeImageView(name, image, image_priv->create_info.format, mip, 1);
}

RenderGraphImageView RenderGraphBuilder::makeImageView(std::string_view name, RenderGraphImage &image, VkFormat format,
	uint32_t firstMip, uint32_t mipCount)
{
	auto *image_priv = image.getPrivate();
	if (!image_priv) {
		return {};
	}

	auto &priv = image_priv->views.emplace_back();

	priv.name = fmt::format("graph/img/{}", name);
	priv.image = image_priv;

	// TODO: make view type explicitly requestable
	VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;
	if (image_priv->create_info.arrayLayers > 1) {
		view_type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	}

	VkImageSubresourceRange view_range = {
		.aspectMask = formatToAspect(format),
		.baseMipLevel = firstMip,
		.levelCount = mipCount == VK_REMAINING_MIP_LEVELS ? image_priv->create_info.mipLevels : mipCount,
		.baseArrayLayer = 0,
		.layerCount = image_priv->create_info.arrayLayers,
	};

	initImageViewCreateInfo(priv.create_info, priv.usage_create_info, view_type, format, view_range);

	auto *prev_image_priv = image_priv->temporal_sibling;
	if (prev_image_priv) {
		auto &prev_priv = prev_image_priv->views.emplace_back();

		prev_priv.name = priv.name + "@B";
		prev_priv.image = prev_image_priv;
		prev_priv.temporal_sibling = &priv;

		priv.name += "@A";
		priv.temporal_sibling = &prev_priv;

		initImageViewCreateInfo(prev_priv.create_info, prev_priv.usage_create_info, view_type, format, view_range);
	}

	return RenderGraphImageView(priv);
}

RenderGraphBuilder::ResourceUsage RenderGraphBuilder::makeBufferUsage(RenderGraphBuffer &buffer,
	VkPipelineStageFlags2 stages, VkAccessFlags2 access, bool discard)
{
	return {
		.buffer = buffer.getPrivate(),
		.image_view = nullptr,
		.stages = stages,
		.access = access,
		.layout = VK_IMAGE_LAYOUT_UNDEFINED,
		.discard = discard,
	};
}

RenderGraphBuilder::ResourceUsage RenderGraphBuilder::makeImageViewUsage(RenderGraphImageView &view,
	VkPipelineStageFlags2 stages, VkAccessFlags2 access, VkImageLayout layout, bool discard)
{
	return {
		.buffer = nullptr,
		.image_view = view.getPrivate(),
		.stages = stages,
		.access = access,
		.layout = layout,
		.discard = discard,
	};
}

RenderGraphBuilder::RenderTarget RenderGraphBuilder::makeRenderTargetDiscardStore(RenderGraphImage &image, uint32_t mip)
{
	if (!image.getPrivate()) {
		return {};
	}

	RenderGraphImageView view = makeSingleMipImageView(image.getPrivate()->name + "/rtv", image, mip);

	return {
		.resource = view.getPrivate(),
		.load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.store_op = VK_ATTACHMENT_STORE_OP_STORE,
	};
}

RenderGraphBuilder::RenderTarget RenderGraphBuilder::makeRenderTargetClearStore(RenderGraphImage &image,
	VkClearColorValue clear_value, uint32_t mip)
{
	if (!image.getPrivate()) {
		return {};
	}

	RenderGraphImageView view = makeSingleMipImageView(image.getPrivate()->name + "/rtv", image, mip);

	return {
		.resource = view.getPrivate(),
		.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.store_op = VK_ATTACHMENT_STORE_OP_STORE,
		.clear_value = clear_value,
	};
}

RenderGraphBuilder::DepthStencilTarget RenderGraphBuilder::makeDepthStencilTargetClearStore(RenderGraphImage &image,
	VkClearDepthStencilValue clear_value, uint32_t mip)
{
	if (!image.getPrivate()) {
		return {};
	}

	RenderGraphImageView view = makeSingleMipImageView(image.getPrivate()->name + "/dsv", image, mip);

	return {
		.resource = view.getPrivate(),
		.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.store_op = VK_ATTACHMENT_STORE_OP_STORE,
		.clear_value = clear_value,
	};
}

void RenderGraphBuilder::makeComputePass(std::string name, PassCallback callback, std::span<const ResourceUsage> usage)
{
	resolveResourceUsage(usage);

	m_private.commands.emplace_back(RenderGraphPrivate::ComputePassCommand {
		.name = std::move(name),
		.callback = callback,
	});
}

void RenderGraphBuilder::makeRenderPass(std::string name, PassCallback callback,
	std::span<const RenderTarget> color_targets, DepthStencilTarget ds_target, std::span<const ResourceUsage> usage)
{
	resolveResourceUsage(usage);

	RenderGraphPrivate::RenderPassCommand cmd {
		.name = std::move(name),
		.callback = callback,
		.targets = {},
		.ds_target = ds_target,
	};

	if (ds_target.resource) {
		RenderGraphImageView::Private &view = *ds_target.resource;

		assert(view.create_info.subresourceRange.levelCount == 1);

		view.usage_create_info.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		view.image->create_info.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		bool discard = ds_target.load_op == VK_ATTACHMENT_LOAD_OP_CLEAR
			|| ds_target.load_op == VK_ATTACHMENT_LOAD_OP_DONT_CARE;

		resolveImageHazards(view,
			VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
			VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
			ds_target.read_only ? 0 : VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
			discard);
	}

	assert(color_targets.size() <= Consts::GRAPH_MAX_RENDER_TARGETS);
	for (size_t i = 0; i < color_targets.size(); i++) {
		cmd.targets[i] = color_targets[i];

		if (color_targets[i].resource) {
			RenderGraphImageView::Private &view = *color_targets[i].resource;

			assert(view.create_info.subresourceRange.levelCount == 1);

			view.usage_create_info.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			view.image->create_info.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

			bool discard = color_targets[i].load_op == VK_ATTACHMENT_LOAD_OP_CLEAR
				|| color_targets[i].load_op == VK_ATTACHMENT_LOAD_OP_DONT_CARE;

			resolveImageHazards(view, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
				color_targets[i].read_only ? 0 : VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
				discard);
		}
	}

	m_private.commands.emplace_back(std::move(cmd));
}

void RenderGraphBuilder::resolveResourceUsage(std::span<const ResourceUsage> usage)
{
	for (const ResourceUsage &item : usage) {
		assert((item.buffer != nullptr) ^ (item.image_view != nullptr));

		VkAccessFlags2 read_flags = 0;
		VkAccessFlags2 write_flags = 0;
		splitReadWriteFlags(item.access, read_flags, write_flags);

		if (item.buffer) {
			RenderGraphBuffer::Private &buffer = *item.buffer;

			buffer.create_info.usage |= bufferAccessToUsage(item.access);

			resolveBufferHazards(buffer, item.stages, read_flags, write_flags);
			continue;
		}

		RenderGraphImageView::Private &view = *item.image_view;

		VkImageUsageFlags image_usage = imageAccessToUsage(item.access);
		view.usage_create_info.usage |= image_usage;
		view.image->create_info.usage |= image_usage;

		resolveImageHazards(view, item.stages, read_flags, write_flags, item.layout, item.discard);
	}
}

void RenderGraphBuilder::resolveBufferHazards(RenderGraphBuffer::Private &buffer, VkPipelineStageFlags2 new_stages,
	VkAccessFlags2 new_read, VkAccessFlags2 new_write)
{
	if (buffer.write_access == 0 && (buffer.read_access == 0 || new_write == 0)) {
		// No hazards (either not accessed before or Read after Read)
		buffer.stages |= new_stages;
		buffer.read_access |= new_read;
		buffer.write_access |= new_write;
		return;
	}

	// Hazard (Read/Write after Write or Write after Read)
	if (m_private.commands.empty()
		|| !std::holds_alternative<RenderGraphPrivate::BarrierCommand>(m_private.commands.back())) {
		m_private.commands.emplace_back(RenderGraphPrivate::BarrierCommand {});
	}

	auto *cmd = std::get_if<RenderGraphPrivate::BarrierCommand>(&m_private.commands.back());
	assert(cmd);

	cmd->buffer.emplace_back(RenderGraphPrivate::BufferBarrier {
		.buffer = &buffer,
		.src_stages = buffer.stages,
		.src_access = buffer.read_access | buffer.write_access,
		.dst_stages = new_stages,
		.dst_access = new_read | new_write,
	});

	buffer.stages = new_stages;
	buffer.read_access = new_read;
	buffer.write_access = new_write;
}

void RenderGraphBuilder::resolveImageHazards(RenderGraphImageView::Private &view, VkPipelineStageFlags2 new_stages,
	VkAccessFlags2 new_read, VkAccessFlags2 new_write, VkImageLayout new_layout, bool discard)
{
	VkImageSubresourceRange range = view.create_info.subresourceRange;

	VkPipelineStageFlags2 old_stages = 0;
	VkAccessFlags2 old_read = 0;
	VkAccessFlags2 old_write = 0;

	VkImageLayout old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
	bool different_layouts = false;

	uint32_t mipBegin = range.baseMipLevel;
	uint32_t mipEnd = mipBegin + range.levelCount;
	assert(mipEnd <= view.image->create_info.mipLevels);

	for (uint32_t mip = mipBegin; mip < mipEnd; mip++) {
		auto &state = view.image->mip_states[mip];

		if (mip == mipBegin) {
			old_layout = state.layout;
		} else if (state.layout != old_layout) {
			different_layouts = true;
		}

		old_stages |= state.stages;
		old_read |= state.read_access;
		old_write |= state.write_access;
	}

	if (discard) {
		old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
		different_layouts = false;
	}

	if (!different_layouts && old_write == 0 && (old_read == 0 || new_write == 0) && old_layout == new_layout) {
		// No hazards (either not accessed before or Read after Read)
		for (uint32_t mip = mipBegin; mip < mipEnd; mip++) {
			auto &state = view.image->mip_states[mip];

			state.stages |= new_stages;
			state.read_access |= new_read;
			state.write_access |= new_write;
		}
		return;
	}

	// Hazard (Read/Write after Write or Write after Read)
	if (m_private.commands.empty()
		|| !std::holds_alternative<RenderGraphPrivate::BarrierCommand>(m_private.commands.back())) {
		m_private.commands.emplace_back(RenderGraphPrivate::BarrierCommand {});
	}

	auto *cmd = std::get_if<RenderGraphPrivate::BarrierCommand>(&m_private.commands.back());
	assert(cmd);

	if (!different_layouts) {
		cmd->image.emplace_back(RenderGraphPrivate::ImageBarrier {
			.image = view.image,
			.src_stages = old_stages,
			.src_access = old_read | old_write,
			.dst_stages = new_stages,
			.dst_access = new_read | new_write,
			.old_layout = old_layout,
			.new_layout = new_layout,
			.subresource = range,
		});
	} else {
		VkImageSubresourceRange subrange = range;
		subrange.levelCount = 1;

		for (uint32_t mip = mipBegin; mip < mipEnd; mip++) {
			auto &state = view.image->mip_states[mip];
			subrange.baseMipLevel = mip;

			cmd->image.emplace_back(RenderGraphPrivate::ImageBarrier {
				.image = view.image,
				.src_stages = state.stages,
				.src_access = state.read_access | state.write_access,
				.dst_stages = new_stages,
				.dst_access = new_read | new_write,
				.old_layout = state.layout,
				.new_layout = new_layout,
				.subresource = subrange,
			});
		}
	}

	for (uint32_t mip = mipBegin; mip < mipEnd; mip++) {
		auto &state = view.image->mip_states[mip];

		state.layout = new_layout;
		state.stages = new_stages;
		state.read_access = new_read;
		state.write_access = new_write;
	}
}

} // namespace voxen::gfx::vk
