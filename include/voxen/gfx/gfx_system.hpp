#pragma once

#include <voxen/gfx/frame_tick_id.hpp>
#include <voxen/gfx/gfx_fwd.hpp>
#include <voxen/os/os_fwd.hpp>
#include <voxen/svc/svc_fwd.hpp>

#include <memory>

namespace voxen::gfx
{

// This class is a "god object" storing the whole graphics subsystem (Vulkan renderer).
//
// Most of its functions except `drawFrame()` are for internal usage and
// should not (or make no sense to) be called by external parties.
//
// All subsystem components are stored inline in a large pile of bytes
// for the best cache locality. Pointers into the pile are exposed outside
// in `extras::pimpl` style, otherwise this would be an "include-all" hell.
//
// This is NOT a service - it is not intended to be discovered by outside entities.
// Basically, graphics subsystem only consumes information from the rest of the engine.
// Should be created inside `MainThreadService` or in similar place.
class GfxSystem final {
public:
	// RAII-style init - if this constructor did not throw then the graphics subsystem is operational.
	// Window and service locator must remain valid during the whole object lifetime.
	//
	// TODO: support headless mode (creating without a window).
	explicit GfxSystem(svc::ServiceLocator& svc, os::GlfwWindow& main_window);
	GfxSystem(GfxSystem&&) = delete;
	GfxSystem(const GfxSystem&) = delete;
	GfxSystem& operator=(GfxSystem&&) = delete;
	GfxSystem& operator=(const GfxSystem&) = delete;
	~GfxSystem();

	// Acquire the latest observable state from the engine,
	// render and present it into the attached window.
	//
	// You can only call it from the main thread.
	//
	// If it throws an exception, most likely this means a device loss or out of memory.
	// In either case the system becomes unusable and must be either restarted or terminated.
	// Further calls to `drawFrame()` will almost certainly throw too.
	void drawFrame();

	// Wait (block) until the given frame tick ID completes GPU execution.
	// When this function returns, any resource associated with `tick_id`
	// or earlier can be freely released or recycled without CPU-GPU race.
	void waitFrameCompletion(FrameTickId tick_id);

	vk::Instance* instance() noexcept { return m_vk_instance.get(); }
	vk::Device* device() noexcept { return m_vk_device.get(); }
	vk::CommandAllocator *commandAllocator() noexcept { return m_vk_command_allocator.get(); }
	vk::TransientBufferAllocator* transientBufferAllocator() noexcept { return m_vk_transient_buffer_allocator.get(); }
	vk::DmaSystem* dmaSystem() noexcept { return m_vk_dma_system.get(); }
	vk::MeshStreamer* meshStreamer() noexcept { return m_vk_mesh_streamer.get(); }
	vk::RenderGraphRunner* renderGraphRunner() noexcept { return m_vk_render_graph_runner.get(); }

	const FrameTickSource* frameTickSource() const noexcept { return m_frame_tick_source.get(); }

private:
	struct ComponentStorage;

	// Special deleter for component pointer. We store them inline
	// so there is no deallocation, only the destructor call.
	struct ComponentDeleter {
		template<typename T>
		void operator()(T* ptr) noexcept
		{
			ptr->~T();
		}
	};

	template<typename T>
	using ComponentPtr = std::unique_ptr<T, ComponentDeleter>;

	// Provides inline storage for all components for the best cache locality
	std::unique_ptr<ComponentStorage> m_component_storage;

	ComponentPtr<vk::Instance> m_vk_instance;
	ComponentPtr<vk::Device> m_vk_device;
	ComponentPtr<vk::CommandAllocator> m_vk_command_allocator;
	ComponentPtr<vk::TransientBufferAllocator> m_vk_transient_buffer_allocator;
	ComponentPtr<vk::DmaSystem> m_vk_dma_system;
	ComponentPtr<vk::MeshStreamer> m_vk_mesh_streamer;
	ComponentPtr<vk::RenderGraphRunner> m_vk_render_graph_runner;

	ComponentPtr<FrameTickSource> m_frame_tick_source;

	// Notify every tick-synchronized component of a new frame tick + tick completion
	void notifyFrameTickBegin(FrameTickId completed_tick, FrameTickId new_tick);
	// Notify every tick-synchronized component of a frame tick end (to submit all commands from this tick)
	void notifyFrameTickEnd(FrameTickId current_tick);
};

} // namespace voxen::gfx
