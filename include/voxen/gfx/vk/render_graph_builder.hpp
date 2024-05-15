#pragma once

#include <voxen/gfx/vk/render_graph.hpp>
#include <voxen/gfx/vk/render_graph_resource.hpp>
#include <voxen/util/resolution.hpp>

#include <functional>
#include <span>
#include <string>

namespace voxen::gfx::vk
{

struct RenderGraphPrivate;

// A temporary entity passed to `IRenderGraph::rebuild()`.
// Use its interface to declare resources, their usage and compute/render passes.
// NOTE: these declarations are not persistent, everything must
// be declared again on the next call to `IRenderGraph::rebuild()`.
class VOXEN_API RenderGraphBuilder {
public:
	using PassCallback = void (*)(IRenderGraph &, RenderGraphExecution &);

	struct Image2DConfig {
		VkFormat format = VK_FORMAT_UNDEFINED;
		Resolution resolution;
		uint32_t mips = 1;
		uint32_t layers = 1;
	};

	struct ResourceUsage {
		RenderGraphBuffer::Private *buffer = nullptr;
		RenderGraphImageView::Private *image_view = nullptr;

		VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_NONE;
		VkAccessFlags2 access = VK_ACCESS_2_NONE;

		VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
		bool discard = false;
	};

	struct RenderTarget {
		RenderGraphImageView::Private *resource = nullptr;

		VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		VkAttachmentStoreOp store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		VkClearColorValue clear_value = {};

		bool read_only = false;
	};

	struct DepthStencilTarget {
		RenderGraphImageView::Private *resource = nullptr;

		VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		VkAttachmentStoreOp store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		VkClearDepthStencilValue clear_value = {};

		bool read_only = false;
	};

	explicit RenderGraphBuilder(RenderGraphPrivate &priv) noexcept;
	RenderGraphBuilder(RenderGraphBuilder &&) = delete;
	RenderGraphBuilder(const RenderGraphBuilder &) = delete;
	RenderGraphBuilder &operator=(RenderGraphBuilder &&) = delete;
	RenderGraphBuilder &operator=(const RenderGraphBuilder &) = delete;
	~RenderGraphBuilder() noexcept;

	RenderGraphImage &outputImage();

	// Images

	// Declare a 2D image
	RenderGraphImage make2DImage(std::string_view name, Image2DConfig config);
	// Declare a double-buffered 2D image.
	// Returns a pair of images where one is "current" and the other is "previous".
	// The pair is symmetric but `(current; previous)` usage convention is encouraged.
	// Images will swap their handles at the beginning of each graph execution.
	std::pair<RenderGraphImage, RenderGraphImage> makeDoubleBuffered2DImage(std::string_view name, Image2DConfig config);

	// Buffers

	// Declare a fixed-size buffer
	RenderGraphBuffer makeBuffer(std::string_view name, VkDeviceSize size);
	// Declare a dynamic-sized buffer. Its size must be set with `RenderGraphExecution::setDynamicBufferSize()`.
	// NOTE: size must be set on each graph execution. You will receive a valid handle only after that.
	RenderGraphBuffer makeDynamicSizedBuffer(std::string_view name);

	// Image views

	// Declare a view covering the whole image
	RenderGraphImageView makeBasicImageView(std::string_view name, RenderGraphImage &image);
	// Declare a view covering one MIP level of the image
	RenderGraphImageView makeSingleMipImageView(std::string_view name, RenderGraphImage &image, uint32_t mip = 0);
	// Declare a view covering a provided range of MIP levels, possibly with a different format.
	// View type will be VK_IMAGE_VIEW_TYPE_2D for image with only one array layer
	// and VK_IMAGE_VIEW_TYPE_2D_ARRAY otherwise.
	RenderGraphImageView makeImageView(std::string_view name, RenderGraphImage &image, VkFormat format,
		uint32_t firstMip = 0, uint32_t mipCount = VK_REMAINING_MIP_LEVELS);

	// Resource usage

	// Declare buffer usage. If `discard == true` then preserving previous contents is not required.
	ResourceUsage makeBufferUsage(RenderGraphBuffer &buffer, VkPipelineStageFlags2 stages, VkAccessFlags2 access,
		bool discard = false);
	// Declare image view usage. If `discard == true` then preserving previous contents is not required.
	ResourceUsage makeImageViewUsage(RenderGraphImageView &view, VkPipelineStageFlags2 stages, VkAccessFlags2 access,
		VkImageLayout layout, bool discard = false);

	// Shorthand for a very common SRV (shader resource view) use case
	ResourceUsage makeSrvUsage(RenderGraphImageView &view, VkPipelineStageFlags2 stages)
	{
		return makeImageViewUsage(view, stages, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false);
	}

	// Render targets

	// Declare render target with VK_ATTACHMENT_LOAD_OP_DONT_CARE and VK_ATTACHMENT_STORE_OP_STORE
	RenderTarget makeRenderTargetDiscardStore(RenderGraphImage &image, uint32_t mip = 0);
	// Declare render target with VK_ATTACHMENT_LOAD_OP_CLEAR and VK_ATTACHMENT_STORE_OP_STORE
	RenderTarget makeRenderTargetClearStore(RenderGraphImage &image, VkClearColorValue clear_value, uint32_t mip = 0);

	// Declare depth/stencil target with VK_ATTACHMENT_LOAD_OP_CLEAR and VK_ATTACHMENT_STORE_OP_STORE
	DepthStencilTarget makeDepthStencilTargetClearStore(RenderGraphImage &image, VkClearDepthStencilValue clear_value,
		uint32_t mip = 0);

	// Passes (will be executed in declaration order)

	// Declare a compute pass. Proper synchronization operations according to `usage`
	// will be automatically recorded into command buffer before calling `callback`.
	void makeComputePass(std::string name, PassCallback callback, std::span<const ResourceUsage> usage);
	// Declare a render pass. Proper synchronization operations according to `usage`,
	// as well as `vkCmdBeginRendering` will be automatically recorded into
	// command buffer before calling `callback` (and `vkCmdEndRendering` after it).
	// NOTE: do not mention render targets in `usage`.
	void makeRenderPass(std::string name, PassCallback callback, std::span<const RenderTarget> color_targets,
		DepthStencilTarget ds_target, std::span<const ResourceUsage> usage);

	// Helper to create compute pass with pointer to member function callback:
	// `bld.makeComputePass<&MyRenderGraph::fooPassCallback("foo", ...)`
	template<auto F>
	void makeComputePass(std::string name, std::span<const ResourceUsage> usage)
	{
		makeComputePass(
			std::move(name),
			[](IRenderGraph &me, RenderGraphExecution &exec) {
				std::invoke(F, static_cast<typename decltype(ClassTypeHelper(F))::type>(me), exec);
			},
			usage);
	}

	// Helper to create render pass with pointer to member function callback:
	// `bld.makeRenderPass<&MyRenderGraph::barPassCallback("bar", ...)`
	template<auto F>
	void makeRenderPass(std::string name, std::span<const RenderTarget> color_targets, DepthStencilTarget ds_target,
		std::span<const ResourceUsage> usage)
	{
		makeRenderPass(
			std::move(name),
			[](IRenderGraph &me, RenderGraphExecution &exec) {
				std::invoke(F, static_cast<typename decltype(ClassTypeHelper(F))::type>(me), exec);
			},
			color_targets, ds_target, usage);
	}

private:
	RenderGraphPrivate &m_private;

	// Helper to infer class type from pointer to member function
	template<typename T, typename U>
	struct ClassTypeHelper {
		ClassTypeHelper(U T::*) {}

		using type = T &;
	};

	void resolveResourceUsage(std::span<const ResourceUsage> usage);
	void resolveBufferHazards(RenderGraphBuffer::Private &buffer, VkPipelineStageFlags2 new_stages,
		VkAccessFlags2 new_read, VkAccessFlags2 new_write);
	void resolveImageHazards(RenderGraphImageView::Private &view, VkPipelineStageFlags2 new_stages,
		VkAccessFlags2 new_read, VkAccessFlags2 new_write, VkImageLayout new_layout, bool discard);
};

} // namespace voxen::gfx::vk
