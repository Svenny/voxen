#pragma once

namespace voxen::gfx
{

class FontRenderer;
class FrameTickSource;
class GfxSystem;
struct GfxOptions;
class LandLoader;

namespace detail
{

class LandLoaderImpl;

}

namespace graph
{

class IRenderPass;
class RenderGraphBuilder;
class RenderGraphExecution;
class RenderGraphPreparation;

} // namespace graph

namespace vk
{

class CommandAllocator;
class DebugUtils;
class DescriptorSetLayoutCollection;
class Device;
class DmaSystem;
class FrameContext;
class Instance;
class IRenderGraph;
class MeshStreamer;
class PhysicalDevice;
class PipelineCache;
class PipelineCollection;
class PipelineLayoutCollection;
class RenderGraphBuilder;
class RenderGraphExecution;
class RenderGraphRunner;
class Swapchain;
class TransientBufferAllocator;

} // namespace vk

} // namespace voxen::gfx
