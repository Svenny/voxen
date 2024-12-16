#pragma once

namespace voxen::gfx
{

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
class Instance;
class IRenderGraph;
class MeshStreamer;
class PhysicalDevice;
class RenderGraphRunner;
class Swapchain;
class TransientBufferAllocator;

} // namespace vk

} // namespace voxen::gfx
