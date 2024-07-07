#pragma once

#include <voxen/gfx/vk/frame_context.hpp>
#include <voxen/gfx/vk/render_graph_builder.hpp>
#include <voxen/gfx/vk/render_graph_resource.hpp>
#include <voxen/gfx/vk/vk_device.hpp>

#include "vk_private_consts.hpp"

#include <deque>
#include <vector>

namespace voxen::gfx::vk
{

struct VOXEN_LOCAL RenderGraphBuffer::Private {
	RenderGraphBuffer *resource = nullptr;

	std::string name;
	bool dynamic_sized = false;
	VkDeviceSize used_size = 0;

	VkPipelineStageFlags2 stages = 0;
	VkAccessFlags2 read_access = 0;
	VkAccessFlags2 write_access = 0;

	VkBufferCreateInfo create_info = {};
	VkBuffer handle = VK_NULL_HANDLE;
	VmaAllocation alloc = VK_NULL_HANDLE;
};

struct VOXEN_LOCAL RenderGraphImageView::Private {
	RenderGraphImageView *resource = nullptr;
	RenderGraphImage::Private *image = nullptr;
	Private *temporal_sibling = nullptr;

	std::string name;

	VkImageViewUsageCreateInfo usage_create_info = {};
	VkImageViewCreateInfo create_info = {};
	VkImageView handle = VK_NULL_HANDLE;
};

struct VOXEN_LOCAL RenderGraphImage::Private {
	struct MipState {
		VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkPipelineStageFlags2 stages = 0;
		VkAccessFlags2 read_access = 0;
		VkAccessFlags2 write_access = 0;
	};

	RenderGraphImage *resource = nullptr;
	Private *temporal_sibling = nullptr;

	std::string name;
	std::deque<RenderGraphImageView::Private> views;

	std::vector<MipState> mip_states;

	VkImageCreateInfo create_info = {};
	VkImage handle = VK_NULL_HANDLE;
	VmaAllocation alloc = VK_NULL_HANDLE;
};

// Collection of render graph resources and commands
struct VOXEN_LOCAL RenderGraphPrivate {
	explicit RenderGraphPrivate(Device &device) : device(device), fctx_ring(device, Consts::GRAPH_CONTEXT_RING_SIZE) {}
	RenderGraphPrivate(RenderGraphPrivate &&) = delete;
	RenderGraphPrivate(const RenderGraphPrivate &) = delete;
	RenderGraphPrivate &operator=(RenderGraphPrivate &&) = delete;
	RenderGraphPrivate &operator=(const RenderGraphPrivate &) = delete;
	~RenderGraphPrivate() noexcept;

	struct BufferBarrier {
		RenderGraphBuffer::Private *buffer = nullptr;
		VkPipelineStageFlags2 src_stages = 0;
		VkAccessFlags2 src_access = 0;
		VkPipelineStageFlags2 dst_stages = 0;
		VkAccessFlags2 dst_access = 0;
	};

	struct ImageBarrier {
		RenderGraphImage::Private *image = nullptr;
		VkPipelineStageFlags2 src_stages = 0;
		VkAccessFlags2 src_access = 0;
		VkPipelineStageFlags2 dst_stages = 0;
		VkAccessFlags2 dst_access = 0;
		VkImageLayout old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkImageLayout new_layout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkImageSubresourceRange subresource = {};
	};

	struct BarrierCommand {
		std::vector<BufferBarrier> buffer;
		std::vector<ImageBarrier> image;
	};

	struct RenderPassCommand {
		std::string name;
		RenderGraphBuilder::PassCallback callback;
		RenderGraphBuilder::RenderTarget targets[Consts::GRAPH_MAX_RENDER_TARGETS];
		RenderGraphBuilder::DepthStencilTarget ds_target;
	};

	struct ComputePassCommand {
		std::string name;
		RenderGraphBuilder::PassCallback callback;
	};

	using Command = std::variant<BarrierCommand, RenderPassCommand, ComputePassCommand>;

	Device &device;
	FrameContextRing fctx_ring;

	// Private parts of buffer resources. Using deque to always preserve pointers.
	std::deque<RenderGraphBuffer::Private> buffers;
	// Private parts of image resources. Using deque to always preserve pointers.
	std::deque<RenderGraphImage::Private> images;
	// High-level "commands" defining the graph execution.
	std::vector<Command> commands;

	RenderGraphImage output_image;
};

} // namespace voxen::gfx::vk
