#pragma once

namespace voxen::gfx
{

class FontRenderer;
class FrameTickSource;
class GfxSystem;
class LandLoader;

namespace detail
{

class LandLoaderImpl;

}

namespace vk
{

class CommandAllocator;
class DebugUtils;
class Device;
class DmaSystem;
class FrameContext;
class Instance;
class IRenderGraph;
class MeshStreamer;
class PhysicalDevice;
class RenderGraphBuilder;
class RenderGraphExecution;
class RenderGraphRunner;
class Swapchain;
class TransientBufferAllocator;

} // namespace vk

} // namespace voxen::gfx
